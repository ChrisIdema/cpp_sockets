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


class Simple_socket_library
{
    public:
    Simple_socket_library()
    {
        m_initialized = false;
        int res = WSAStartup(MAKEWORD(2, 2), &m_wsa);

        if (res != 0)
        {
            printf("WSAStartup failed with error: %d\n", res);
        }
        else
        {          
            if (LOBYTE(m_wsa.wVersion) != 2 || HIBYTE(m_wsa.wVersion) != 2) 
            {
                printf("Could not find a usable version of Winsock.dll\n");
                WSACleanup();
            }
            else
            {
                printf("WSA initialized\n");
                m_initialized = true;
            }
        }
    }

    ~Simple_socket_library()
    {
        if (m_initialized)
        {
            WSACleanup();
        }
    }

    private:
    WSADATA m_wsa;
    bool m_initialized;
    
};



#ifdef _WIN64
#define SOCKET_FORMAT_STRING "%llu"
#else
#define SOCKET_FORMAT_STRING "%u"
#endif

#else

typedef int Simple_socket_library;

#define SOCKET_FORMAT_STRING "%d"

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


//raw socket only stores native socket type and has no other state, reading error codes may not be thread safe, no automatic closing
class Raw_socket
{
    public:
    Raw_socket(SOCKET raw_socket=INVALID_SOCKET)
    : m_socket(raw_socket)
    {

    }

    Raw_socket(int family, int socket_type, int protocol)
    {
        m_socket = ::socket(family, socket_type, protocol);
    }


    SOCKET accept(struct sockaddr* addr, socklen_t* addrlen)
    {
        SOCKET s = INVALID_SOCKET;

        if(valid())
        {
            s = ::accept(m_socket, addr, addrlen);
        }

        return s;
    }
    
    bool valid() const{ return m_socket != INVALID_SOCKET;}
    SOCKET get_native() const{return m_socket;}

    bool operator==(const Raw_socket& rhs) const {return m_socket == rhs.m_socket;}
    bool operator!=(const Raw_socket& rhs) const {return m_socket != rhs.m_socket;}

    int close()
    {
        int res = SOCKET_ERROR;
        if (valid())
        {            
            #ifdef WIN32
                res = ::closesocket(m_socket);
            #else
                res = ::close(m_socket);
            #endif

            m_socket = INVALID_SOCKET;
        }

        return res;
    }

    int bind(const struct sockaddr* name, int namelen)
    {
        int res = ::bind(m_socket, name, namelen);
        return res;
    }

    int connect(const struct sockaddr* name, int namelen)
    {
        int res = ::connect(m_socket, name, namelen);
        return res;
    }

    void print() const
    {
        if (valid())
        {            
            printf(SOCKET_FORMAT_STRING"\n", m_socket);
        }
        else
        {
            printf("INVALID_SOCKET\n");
        }
    }

    int setsockop_bool(int optname, bool value) const
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

    int listen(int backlog) const
    {  
        int res = SOCKET_ERROR;
        if (valid())
        {
            res = ::listen(m_socket, backlog);
        }

        return res;
    }


    int send(const char* buffer, size_t length, int flags=0) const
    {
        int res = SOCKET_ERROR;
        if (valid() && buffer != nullptr && length>0 && length<=INT32_MAX)
        {
            res = ::send(m_socket, buffer, length, flags);
        }  
        return res;    
    }

    int recv(char* buffer, size_t length, int flags=0) const
    {
        int res = SOCKET_ERROR;
        if (valid() && buffer != nullptr && length>0 && length<=INT32_MAX)
        {
            res = ::recv(m_socket, buffer, length, flags);
        }
        
        return res;
    }

    int get_last_error() const
    {
        #ifdef _WIN32
        return WSAGetLastError();
        #else
        return errno;
        #endif
    }

    // static void print_error(int error)
    // {
    //     //https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
    // }

    private:
    SOCKET m_socket;
};



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
            m_socket = Raw_socket(AF_INET, SOCK_STREAM, 0);

            printf("m_socket: ");
            m_socket.print();

            if (!m_socket.valid())
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

        struct addrinfo hints={0};
        struct addrinfo *servinfo=nullptr;
        int res;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        res = getaddrinfo(m_server_ip.c_str(), std::to_string(m_server_port).c_str(), &hints, &servinfo);
        if (res != 0) 
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
            return m_initialized;
        }

        // loop through all the results and connect to the first we can
        for(auto p = servinfo; p != nullptr; p = p->ai_next) 
        {
            // SOCKET temp_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            Raw_socket temp_socket(p->ai_family, p->ai_socktype, p->ai_protocol);

            if (!temp_socket.valid()) 
            {
                perror("client: socket");
                continue;
            }

            res = temp_socket.connect(p->ai_addr, p->ai_addrlen);

            if (res == SOCKET_ERROR) 
            {
                temp_socket.close();

                perror("client: connect");
                continue;
            }
            else
            {
                m_socket = temp_socket;
                char s[INET6_ADDRSTRLEN]="";
                inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, INET6_ADDRSTRLEN);
                printf("client: connecting to %s\n", s);
                m_initialized = true;
                break;
            }            
        }

        freeaddrinfo(servinfo); // all done with this structure
        servinfo = nullptr;

        if (!m_socket.valid())
        {
            fprintf(stderr, "client: failed to connect\n");
            return m_initialized;
        }

    
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
            m_socket.close();
        }
    }



    int set_options()
    {
        int res;
        res = m_socket.setsockop_bool(SO_KEEPALIVE, true);
        if (res != SOCKET_ERROR)
        {
            res = m_socket.setsockop_bool(SO_REUSEADDR, true);
        }

        printf("setsockopt res: %d\n", res);
        //printf("get_last_error(): %d\n", m_socket.get_last_error());

        return res;
    }

    int bind()
    {
        int wsa_error = 0;
        //int res = ts_bind(m_socket,(const sockaddr *)&m_addr,sizeof(m_addr), &wsa_error);
        int res = m_socket.bind((const sockaddr *)&m_addr, sizeof(m_addr));
        printf("bind res: %d\n", res);
        //printf("get_last_error(): %d\n", m_socket.get_last_error());
        return res;
    }

    int listen(int backlog)
    {  
        int res = SOCKET_ERROR;
        if (m_initialized)
        {
            res = m_socket.listen(backlog);
            printf("listen res: %d\n",res);
            printf("listen m_socket: ");
            m_socket.print();
        }
        else
        {
            printf("not initialized\n");
        }

        return res;
    }

    SOCKET get_native() const
    {
        return m_socket.get_native();
    }

    Raw_socket get_raw_socket() const
    {
        return m_socket;
    }


    int send(const char* buffer, size_t length, int flags=0)
    {
        int res = -1;
        if (m_initialized && !m_server && buffer != nullptr && length>0 && length<=INT32_MAX)
        {
            res = m_socket.send(buffer, length, flags);
        }  
        return res;      
    }

    int recv(char* buffer, size_t length, int flags=0)
    {
        int res = -1;
        if (m_initialized && !m_server && buffer != nullptr && length>0 && length<=INT32_MAX)
        {
            res = m_socket.recv(buffer, length, flags);
        }
        
        return res;
    }

private:

    Raw_socket m_socket;
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

                FD_SET(m_socket.get_native(),&m_socket_set);
                m_socket_list.push_back(m_socket.get_raw_socket());   
                #ifndef _WIN32
                m_largest_fd = m_socket.get_native();
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
        Raw_socket client;
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
            fd_set event_socket_set = m_socket_set; //select modifies set, so make a copy
            
            #if defined(_WIN32)
                int res = select(0, &event_socket_set, NULL, NULL, NULL);
            #else
                int res = select(m_largest_fd+1, &event_socket_set, NULL, NULL, NULL);
            #endif

            printf("select res: %d\n",res);

            auto m_socket_list_copy = m_socket_list;
            for(const auto& raw_socket: m_socket_list_copy)
            {
                printf("checking events for: ");
                raw_socket.print();
                
                if (FD_ISSET(raw_socket.get_native(), &event_socket_set)) //new event 
                { 
                    printf("Event for socket: ");
                    raw_socket.print();

                    if (raw_socket == m_socket.get_raw_socket()) //server event
                    {                    
                        struct sockaddr_storage remoteaddr; // client address                   
                        socklen_t addrlen = sizeof(remoteaddr);    

                        //printf("server_event\n");
                        Raw_socket new_socket = m_socket.get_raw_socket().accept((struct sockaddr *)&remoteaddr, &addrlen);

                        if (!new_socket.valid())
                        {
                            //printf("accept failed: %d\n", new_socket.get_last_error());
                        } 
                        else 
                        {
                            FD_SET(new_socket.get_native(), &m_socket_set); // add to master set
                            m_socket_list.push_back(new_socket);  

                            #ifndef _WIN32
                                if (new_socket.get_native() > m_largest_fd)
                                {
                                    m_largest_fd = new_socket.get_native();
                                }                            
                            #endif
             
                            char remoteIP[INET6_ADDRSTRLEN];
                            const char* address = inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),  remoteIP, INET6_ADDRSTRLEN);
                            uint16_t port = ntohs(get_in_port((struct sockaddr*)&remoteaddr));

                            printf("selectserver: new connection from %s:%u on socket ", address, port);
                            new_socket.print();

                            Event event = {Event_code::client_connected, raw_socket, 0};
                            events.push_back(event);
                        }
                    }
                    else //client event
                    {                        
                        uint8_t buffer[1024];
                        int peek = raw_socket.recv((char*)buffer, sizeof(buffer), MSG_PEEK);

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
                            FD_CLR(raw_socket.get_native(), &m_socket_set);     
                            m_socket_list.remove(raw_socket);  

                            #ifndef _WIN32
                                //if largest has been removed a new one needs to be calculated
                                if (raw_socket.get_native() == m_largest_fd) 
                                {
                                    m_largest_fd = -1;
                                    for(const auto& fd: m_socket_list)
                                    {
                                        if(fd.get_native() > m_largest_fd)
                                        {
                                            m_largest_fd = fd.get_native();
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

    std::list<Raw_socket> get_client_list()
    {
        std::list<Raw_socket> client_list = m_socket_list;
        client_list.remove(m_socket.get_raw_socket());
        return m_socket_list;
    }

    private:

    bool m_initialized;
    Simple_socket m_socket;

    fd_set m_socket_set;
    std::list<Raw_socket> m_socket_list;
    #ifndef _WIN32
    int m_largest_fd;
    #endif
};
