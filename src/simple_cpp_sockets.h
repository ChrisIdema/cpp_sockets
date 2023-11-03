// Windows
#if defined(_WIN32)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mutex>

// Link with ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

typedef SSIZE_T ssize_t;

// Linux
#else
#include <sys/socket.h>
//#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h> // This contains inet_addr
#include <unistd.h> // This contains close
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
typedef int SOCKET;
#endif

#include <string>
#include <vector>
#include <list>

#if defined(_WIN32)

extern std::mutex socket_mutex;
extern WSADATA wsa;

static int simple_socket_init(int* wsa_error=nullptr)
{
    int res;
    res = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (res != 0)
    {
        if (wsa_error != nullptr)
        {
            *wsa_error = WSAGetLastError();
        }
    }

    return res;
}

static void simple_socket_deinit()
{
    WSACleanup();
}

//RAII wrapper for simple_socket_init/simple_socket_deinit
class Simple_socket_library
{
    public:
    Simple_socket_library(int* error=nullptr)
    {
        m_initialized = simple_socket_init(error) == 0;
    }

    ~Simple_socket_library()
    {
        if (m_initialized)
        {
            simple_socket_deinit();
        }
    }

    private:
    bool m_initialized;
};

//thread safe bind function
//for wsa_error codes see: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
static int ts_bind(SOCKET socket, const struct sockaddr* name, int namelen, int* wsa_error=nullptr)
{
    const std::lock_guard<std::mutex> lock(socket_mutex);  // needed because WSAGetLastError might not be thread safe, todo: check
    int res = bind(socket, name, namelen);

    if(res == SOCKET_ERROR);
    {
        if (wsa_error != nullptr)
        {
            *wsa_error = WSAGetLastError();
        }
    }

    return res;
}

//thread safe accept function
//for wsa_error codes see: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
static SOCKET ts_accept(SOCKET socket, struct sockaddr* addr, int* addrlen, int* wsa_error=nullptr)
{
    const std::lock_guard<std::mutex> lock(socket_mutex); // needed because WSAGetLastError might not be thread safe, todo: check

    SOCKET new_socket = accept(socket, addr, addrlen);

    if (new_socket == INVALID_SOCKET)
    {
        if (wsa_error != nullptr)
        {
            *wsa_error = WSAGetLastError();
        }
    }

    return new_socket;
}


#ifdef _WIN64
#define SOCKET_FORMAT_STRING "%llu"
#else
#define SOCKET_FORMAT_STRING "%u"
#endif

#else

#define simple_socket_init() (void)
#define simple_socket_deinit() (void)

typedef int Simple_socket_library;

#define SOCKET_FORMAT_STRING "%d"

static int ts_bind(SOCKET socket, const struct sockaddr* name, int namelen, int* error=nullptr)
{
    //const std::lock_guard<std::mutex> lock(socket_mutex);  // needed because WSAGetLastError might not be thread safe, todo: check
    int res = bind(socket, name, namelen);

    if(res == SOCKET_ERROR);
    {
        if (error != nullptr)
        {
            //*error = errno();
        }
    }

    return res;
}

//thread safe accept function
//for wsa_error codes see: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
static SOCKET ts_accept(SOCKET socket, struct sockaddr* addr, socklen_t* addrlen, int* error=nullptr)
{
    //const std::lock_guard<std::mutex> lock(socket_mutex); // needed because WSAGetLastError might not be thread safe, todo: check

    SOCKET new_socket = accept(socket, addr, addrlen);

    if (new_socket == INVALID_SOCKET)
    {
        if (error != nullptr)
        {
            //*error = errno();
        }        
    }

    return new_socket;
}
#endif

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static uint16_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return ((struct sockaddr_in*)sa)->sin_port;
    }

    return ((struct sockaddr_in6*)sa)->sin6_port;
}

static inline void print_socket(SOCKET s)
{
    if (s==INVALID_SOCKET)
    {
        printf("INVALID_SOCKET\n");
    }
    else
    {
        printf(SOCKET_FORMAT_STRING"\n",s);
    }
}



class Simple_socket {
public:
    Simple_socket(bool server=false)
        : 
        m_socket(INVALID_SOCKET),
        m_addr({0}),
        m_address_valid(false),
        m_initialized(false),
        m_server(server),
        m_server_ip(""),
        m_server_port(0)
    {
    }

    Simple_socket(bool server, std::string ip_address, uint16_t port)
        : 
        m_socket(INVALID_SOCKET),
        m_addr({0}),
        m_address_valid(false),
        m_initialized(false),
        m_server(server),
        m_server_ip(ip_address),
        m_server_port(port)
    {
        if (m_server)
        {
            set_address_server();          
        }
        else // client
        {
            m_address_valid = true;
        }
    }

    void set_address_server()
    {
        //m_server_ip.clear();
        int res = inet_pton(AF_INET, m_server_ip.c_str(), &m_addr.sin_addr);
        m_addr.sin_port = htons(m_server_port);
        m_address_valid = res != INVALID_SOCKET; 
        printf("inet_pton res: %d\n",res);  
        //m_server_ip.clear();  
    }


    ~Simple_socket()
    {
        close();
    }

    bool init_server(void)
    {
        if (m_initialized)
        {
            return true;
        }

        if (m_address_valid)
        {
            SOCKET socket = ::socket(AF_INET, SOCK_STREAM, 0);  
            //m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (socket == INVALID_SOCKET)
            {
                m_socket = INVALID_SOCKET;
            }
            else
            {
                m_socket = socket;
            }

            printf("m_socket: ");
            print_socket(m_socket);

            if (m_socket == INVALID_SOCKET)
            {
                printf("error\n");
                //printf("WSAGetLastError(): %d\n",WSAGetLastError());
            }
            else
            {
                m_initialized = true;
                m_addr.sin_family = AF_INET;
                int res = bind();
                res = set_options();  
                //todo process errors
            }
        }
        else
        {
           printf("not initialized\n"); 
        }

        return m_initialized;
    }

    bool init_client(void)
    {
        if (m_initialized)
        {
            return m_initialized;
        }

        char buf[10]="";
        struct addrinfo hints={0};
        struct addrinfo *servinfo=nullptr;
        struct addrinfo *p=nullptr;
        int res;
        char s[INET6_ADDRSTRLEN];


        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        res = getaddrinfo(m_server_ip.c_str(), std::to_string(m_server_port).c_str(), &hints, &servinfo);
        if (res != 0) 
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
            return m_initialized;
        }

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) 
        {
            SOCKET temp_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

            if (temp_socket == INVALID_SOCKET) 
            {
                perror("client: socket");
                continue;
            }

            res = connect(temp_socket, p->ai_addr, p->ai_addrlen);

            if (res == SOCKET_ERROR) 
            {
                #ifdef WIN32
                    closesocket(temp_socket);
                #else
                    close(temp_socket);
                #endif
                perror("client: connect");
                continue;
            }
            else
            {
                m_socket = temp_socket;
                break;
            }            
        }

        if (m_socket == INVALID_SOCKET)
        {
            fprintf(stderr, "client: failed to connect\n");
            return m_initialized;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
        printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure

        m_initialized = true;

        return m_initialized;
        
    }

    bool init(void)
    {
        if (m_server)
        {
            return init_server();
        }
        else
        {
            return init_client();
        }     
    }



    bool init(std::string ip_address, uint16_t port)
    {
        if (m_initialized)
        {
            printf("already initialized!\n");
            return true;
        }

        m_server_ip = ip_address;
        m_server_port = port;

        if (m_server)
        {
            set_address_server();
        }
        
        return init();
    }


    void close()
    {
        if (m_initialized)
        {
            #ifdef WIN32
                ::closesocket(m_socket);
            #else
                ::close(m_socket);
            #endif
            m_initialized = false;
            m_socket = INVALID_SOCKET;
        }
    }

    //todo add params

    int setsockop_bool(int optname, bool value)
    {
        int res;
        #ifdef WIN32
        //https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
        const BOOL converted_param = value;        
        res = ::setsockopt(m_socket, SOL_SOCKET, optname, (char *)&converted_param, sizeof(converted_param));
        #else
        const int converted_param = value;
        res = ::setsockopt(m_socket, SOL_SOCKET, optname, &converted_param, sizeof(converted_param));
        #endif
        return res;
    }

    int set_options()
    {
        int res;
        res = setsockop_bool(SO_KEEPALIVE, true);
        if (res != SOCKET_ERROR)
        {
            res = setsockop_bool(SO_REUSEADDR, true);
        }

        printf("setsockopt res: %d\n", res);
        //printf("WSAGetLastError(): %d\n",WSAGetLastError());

        return res;
    }

    int bind()
    {
        int wsa_error = 0;
        int res = ts_bind(m_socket,(const sockaddr *)&m_addr,sizeof(m_addr), &wsa_error);
        printf("bind res: %d\n",res);
        //printf("WSAGetLastError(): %d\n",WSAGetLastError());
        return res;
    }

    int listen(int backlog)
    {  
        int res = -1;
        if (m_initialized)
        {
            res = ::listen(m_socket, backlog);
            printf("listen res: %d\n",res);
            printf("listen m_socket: ");
            print_socket(m_socket);

            //printf("WSAGetLastError(): %d\n",WSAGetLastError());
        }
        else
        {
            printf("not initialized\n");
        }

        return res;
    }

    SOCKET get_raw_socket() const
    {
        return m_socket;
    }
    // void send(void);
    // void recv(void);

private:

    SOCKET m_socket;
    sockaddr_in m_addr;
    std::string m_server_ip;
    uint16_t m_server_port;
    bool m_address_valid;
    bool m_initialized;
    bool m_server;
};



class Server_socket
{
public:
    Server_socket()
        : 
        m_initialized(false),
        m_socket(Simple_socket(true))
        #ifndef _WIN32
        ,m_largest_fd(-1)
        #endif
    {
        FD_ZERO(&m_socket_set); // clear set
    }

    ~Server_socket()
    {
        //socket will be closed automatically
        //close select?
    }


    void init(std::string ip_address, uint16_t port, uint8_t number_of_connections=10)
    {
        if(!m_initialized)
        {        
            printf("calling m_socket.init\n");
            m_initialized = m_socket.init(ip_address, port);
            if (m_initialized)
            {    
                int res = m_socket.listen(number_of_connections);

                FD_SET(m_socket.get_raw_socket(),&m_socket_set);
                m_socket_list.push_back(m_socket.get_raw_socket());   
                #ifndef _WIN32
                m_largest_fd = m_socket.get_raw_socket();
                #endif
            }
        }
    }

    enum class Event_code
    {
        no_event=0,
        not_initialized,
        client_connected,
        client_disconnected,
        client_error,
        rx,    
    };

    

    struct Event
    {
        Event_code event_code;
        SOCKET client;
        int bytes_available;      
        std::string to_string() const
        {
            std::string s = "0";
            s[0] += int(event_code);
            return s;
        }  
    };

    std::vector<Event> wait_for_events()
    {
        std::vector<Event> events;

        if (m_initialized)
        {
            fd_set event_socket_set = m_socket_set;

            #if defined(_WIN32)
                int res = select(0, &event_socket_set, NULL, NULL, NULL);
            #else
                int res = select(m_largest_fd+1, &event_socket_set, NULL, NULL, NULL);
            #endif

            printf("select res: %d\n",res);
            //printf("WSAGetLastError(): %d\n",WSAGetLastError());


            auto m_socket_list_copy = m_socket_list;
            for(const auto& raw_socket: m_socket_list_copy)
            {
                printf("checking events for: ");
                print_socket(raw_socket);
                
                if (FD_ISSET(raw_socket, &event_socket_set)) //new event 
                { 
                    printf("Event for socket: ");
                    print_socket(raw_socket);

                    if (raw_socket == m_socket.get_raw_socket()) //server event
                    {                    
                        struct sockaddr_storage remoteaddr; // client address
                        //struct sockaddr_in remoteaddr; // client address                        
                        socklen_t addrlen = sizeof(remoteaddr);    
                        int wsa_error = 0;
                        SOCKET res = ts_accept(m_socket.get_raw_socket(), (struct sockaddr *)&remoteaddr, &addrlen, &wsa_error);

                        printf("server_event\n");

                        if (res == INVALID_SOCKET)
                        {
                            printf("accept failed: %d\n", wsa_error);
                        } 
                        else 
                        {
                            SOCKET new_socket = res; 
                            FD_SET(new_socket, &m_socket_set); // add to master set
                            m_socket_list.push_back(new_socket);  

                            #ifndef _WIN32
                                if (new_socket > m_largest_fd)
                                {
                                    m_largest_fd = new_socket;
                                }                            
                            #endif
             
                            char remoteIP[INET6_ADDRSTRLEN];
                            const char* address = inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),  remoteIP, INET6_ADDRSTRLEN);
                            uint16_t port = ntohs(get_in_port((struct sockaddr*)&remoteaddr));

                            printf("selectserver: new connection from %s:%u on socket ", address, port);
                            print_socket(new_socket);

                            Event event = {Event_code::client_connected, raw_socket, 0};
                            events.push_back(event);
                        }
                    }
                    else //client event
                    {                        
                        uint8_t buffer[1024];
                        int peek = recv(raw_socket, (char*)buffer, sizeof(buffer), MSG_PEEK);

                        if (peek < 0)
                        {
                            //error

                            Event event = {Event_code::client_error, raw_socket, 0};
                            events.push_back(event);
                            // FD_CLR(raw_socket, &m_socket_set);     
                            // m_socket_list.remove(raw_socket);    
                        }
                        else if (peek == 0)
                        {
                            //close

                            Event event = {Event_code::client_disconnected, raw_socket, 0};
                            events.push_back(event);
                            FD_CLR(raw_socket, &m_socket_set);     
                            m_socket_list.remove(raw_socket);  

                            #ifndef _WIN32
                                //if largest has been removed a new one needs to be calculated
                                if (raw_socket == m_largest_fd) 
                                {
                                    m_largest_fd = -1;
                                    for(const auto& fd: m_socket_list)
                                    {
                                        if(fd > m_largest_fd)
                                        {
                                            m_largest_fd = fd;
                                        }
                                    }
                                }                            
                            #endif                     
                        }
                        else
                        {
                            //data                            
                            Event event = {Event_code::rx, raw_socket, peek};
                            events.push_back(event); 
                        }
                    }
                }
            }

        }
        else{
            Event event= {Event_code::not_initialized};
            events.push_back(event);
        }

        return events;
    }

    std::list<SOCKET> get_client_list()
    {
        std::list<SOCKET> client_list = m_socket_list;
        client_list.remove(m_socket.get_raw_socket());
        return m_socket_list;
    }

    private:

    bool m_initialized;
    Simple_socket m_socket;

    fd_set m_socket_set;
    std::list<SOCKET> m_socket_list;
    #ifndef _WIN32
    int m_largest_fd;
    #endif
};
