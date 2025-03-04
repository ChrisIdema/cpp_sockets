// Windows
#if defined(_WIN32)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <Ws2tcpip.h>

#define SOL_TCP IPPROTO_TCP

// Link with ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

typedef SSIZE_T ssize_t;

// Linux
#else
#include <netdb.h>      // for getaddrinfo(), gai_strerror()
#include <arpa/inet.h>  // for inet_pton(), inet_ntop()
#include <unistd.h>     // for read(),write(),close()
#include <fcntl.h>      // for fcntl() for self pipe
#include <netinet/tcp.h> // for SOL_TCP
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
typedef int SOCKET; // native type for a socket
#endif


#include <string>
#include <vector>
#include <list>
#include <mutex>
#include <cstring>

#ifdef SIMPLE_CPP_SOCKETS_DEBUG_PRINT
void simple_cpp_sockets_printf(const char * pString, ...);
#define PRINT(...) simple_cpp_sockets_printf(__VA_ARGS__)
#else
#define PRINT(...)
#endif

#if defined(_WIN32)

//instance to run this library (needed on Windows only, optional for Linux)
class Simple_socket_library
{
    public:
    Simple_socket_library()
    {
        m_initialized = false;
        int res = WSAStartup(MAKEWORD(2, 2), &m_wsa);

        if (res != 0)
        {
            PRINT("WSAStartup failed with error: %d\n", res);
        }
        else
        {
            if (LOBYTE(m_wsa.wVersion) != 2 || HIBYTE(m_wsa.wVersion) != 2)
            {
                PRINT("Could not find a usable version of Winsock.dll\n");
                WSACleanup();
            }
            else
            {
                PRINT("WSA initialized\n");
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

#else // Linux
//instance to run this library (needed on Windows only, optional for Linux)
#define Simple_socket_library [[maybe_unused]]int
#define SOCKET_FORMAT_STRING "%d"
#endif

// get sockaddr, IPv4 or IPv6:
static inline void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &reinterpret_cast<sockaddr_in*>(sa)->sin_addr;
    }

    return &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr;
}

static inline uint32_t in_addr_to_uint32(struct sockaddr *sa)
{

    if (sa->sa_family == AF_INET)
    {
        return ntohl(reinterpret_cast<sockaddr_in*>(sa)->sin_addr.s_addr);
    }
    {
        return 0;
    }
}

static inline uint16_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return ((struct sockaddr_in*)sa)->sin_port;
    }

    return ((struct sockaddr_in6*)sa)->sin6_port;
}


//basic wrapper for the native socket type to aid in type checking and easier syntax. It has no member other than the native socket. Purposely doesn't close upon destruction.
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
            #ifdef _WIN32
                res = ::closesocket(m_socket);
            #else
                res = ::close(m_socket);
            #endif

            m_socket = INVALID_SOCKET;
        }

        return res;
    }

    void mark_as_closed()
    {
        m_socket = INVALID_SOCKET;
    }

    int bind(const struct sockaddr* name, size_t namelen)
    {
        int res = ::bind(m_socket, name, int(namelen));
        return res;
    }

    int connect(const struct sockaddr* name, size_t namelen)
    {
        int res = ::connect(m_socket, name, int(namelen));
        return res;
    }

    void print() const
    {
        if (valid())
        {
            PRINT(SOCKET_FORMAT_STRING"\n", m_socket);
        }
        else
        {
            PRINT("INVALID_SOCKET\n");
        }
    }

    int setsockop_bool(int optname, bool value, int level = SOL_SOCKET) const
    {
        int res;
        #ifdef _WIN32
        //https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
        //https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
        if(level == SOL_SOCKET)
        {
            const BOOL converted_param = value;
            res = ::setsockopt(m_socket, level, optname, (char *)&converted_param, sizeof(converted_param));
        }
        else if (level == SOL_TCP)
        {
            if(optname != TCP_ICMP_ERROR_INFO)
            {
                const DWORD converted_param = value;
                res = ::setsockopt(m_socket, level, optname, (char *)&converted_param, sizeof(converted_param));
            }
            else
            {
                res = SOCKET_ERROR;
            }
        }

        #else
        const int converted_param = value;
        res = ::setsockopt(m_socket, level, optname, &converted_param, sizeof(converted_param));
        #endif
        return res;
    }


    int getsockop_bool(int optname, bool *value, int level = SOL_SOCKET) const
    {
        int res;
        #ifdef _WIN32
        //https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
        //https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
        if(level == SOL_SOCKET)
        {
            const BOOL converted_param = false;
            socklen_t optlen = sizeof(converted_param);
            res = ::getsockopt(m_socket, level, optname, (char *)&converted_param, &optlen);
            *value = converted_param;
        }
        else if (level == SOL_TCP)
        {
            if(optname != TCP_ICMP_ERROR_INFO)
            {
                const DWORD converted_param = false;
                socklen_t optlen = sizeof(converted_param);
                res = ::getsockopt(m_socket, level, optname, (char *)&converted_param, &optlen);
                *value = converted_param;
            }
            else
            {
                res = SOCKET_ERROR;
                *value = false;
            }
        }

        #else
        int converted_param = false;
        socklen_t optlen = sizeof(converted_param);
        res = ::getsockopt(m_socket, level, optname, &converted_param, &optlen);
        *value = converted_param;
        #endif
        return res;
    }

    int getsockop_int(int optname, int *value, int level = SOL_SOCKET) const
    {
        int res;
        #ifdef _WIN32
        //https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
        //https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
        if(level == SOL_SOCKET)
        {
            BOOL converted_param = false;
            socklen_t optlen = sizeof(converted_param);
            res = ::getsockopt(m_socket, level, optname, (char *)&converted_param, &optlen);
            *value = converted_param;
        }
        else if (level == SOL_TCP)
        {
            if(optname != TCP_ICMP_ERROR_INFO)
            {
                DWORD converted_param = 0;
                socklen_t optlen = sizeof(converted_param);
                res = ::getsockopt(m_socket, level, optname, (char *)&converted_param, &optlen);
                *value = converted_param;
            }
            else
            {
                res = SOCKET_ERROR;
                *value = 0;
            }
        }

        #else
        int converted_param = 0;
        socklen_t optlen = sizeof(converted_param);
        res = ::getsockopt(m_socket, level, optname, &converted_param, &optlen);
        *value = converted_param;
        #endif
        return res;
    }


    int setsockop_int(int optname, int value, int level = SOL_SOCKET) const
    {
        int res;
        #ifdef _WIN32
        //https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt
        //https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
        if(level == SOL_SOCKET)
        {
            const int converted_param = value;
            res = ::setsockopt(m_socket, level, optname, (char *)&converted_param, sizeof(converted_param));
        }
        else if (level == SOL_TCP)
        {
            if(optname != TCP_ICMP_ERROR_INFO)
            {
                const DWORD converted_param = value;
                res = ::setsockopt(m_socket, level, optname, (char *)&converted_param, sizeof(converted_param));
            }
            else
            {
                res = SOCKET_ERROR;
            }
        }

        #else
        const int converted_param = value;
        res = ::setsockopt(m_socket, level, optname, &converted_param, sizeof(converted_param));
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


    int send(const char* buffer, int length, int flags=0) const
    {
        int res = SOCKET_ERROR;
        if (valid() && buffer != nullptr && length>0 && length<=INT32_MAX)
        {
            res = ::send(m_socket, buffer, length, flags);
        }
        return res;
    }

    int recv(char* buffer, int length, int flags=0) const
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


//provides easy initialization of its Raw_socket member for server or client. Closes upon destruction.
class Simple_socket {
public:
    Simple_socket(bool server=false)
        :
        m_socket(INVALID_SOCKET),
        m_addr(),
        m_server_ip(""),
        m_server_port(0),
        m_client_bind_ip(""),
        m_client_bind_port(0),
        m_address_valid(false),
        m_initialized(false),
        m_server(server)
    {
    }

    Simple_socket(bool server, std::string ip_address, uint16_t port, std::string client_bind_ip ="", uint16_t client_bind_port = 0)
        :
        m_socket(INVALID_SOCKET),
        m_addr(),
        m_server_ip(ip_address),
        m_server_port(port),
        m_client_bind_ip(client_bind_ip),
        m_client_bind_port(client_bind_port),
        m_address_valid(false),
        m_initialized(false),
        m_server(server)
    {
        if (m_server)
        {
            set_address_server();
        }
        else // client
        {
            
        }
    }

    void set_address_server()
    {
        //m_server_ip.clear();
        int res = inet_pton(AF_INET, m_server_ip.c_str(), &m_addr.sin_addr);
        m_addr.sin_port = htons(m_server_port);
        m_address_valid = res != INVALID_SOCKET;
        PRINT("inet_pton res: %d\n",res);
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

            PRINT("m_socket: ");
            m_socket.print();

            if (!m_socket.valid())
            {
                PRINT("get_last_error(): %d\n", m_socket.get_last_error());
            }
            else
            {
                m_addr.sin_family = AF_INET;
                int res = set_options();
                if (res != SOCKET_ERROR)
                {
                    res = bind();
                }

                if (res == SOCKET_ERROR)
                {
                    m_socket.close();
                }

                m_initialized = res != SOCKET_ERROR;
            }
        }
        else
        {
            PRINT("address not initialized\n");
        }

        return m_initialized;
    }

    bool init_client(void)
    {
        if (m_initialized)
        {
            return m_initialized;
        }

        struct addrinfo hints, *info, *info2;
        int res;

        bool bind_to_ip = m_client_bind_ip != "";
        bool bind_to_port = m_client_bind_port != 0;
        auto server_port_string = std::to_string(m_server_port);
        auto client_port_string = std::to_string(m_client_bind_port);

        const char* ip_for_getaddrinfo = bind_to_ip ? m_client_bind_ip.c_str() : m_server_ip.c_str();
        const char* port_for_getaddrinfo = bind_to_port ? client_port_string.c_str() : nullptr; // nullptr means port will be assigned by OS

        // first, load up address structs with getaddrinfo():
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = bind_to_ip? 0 : AI_PASSIVE; //AI_PASSIVE fills in client ip

        res = ::getaddrinfo(ip_for_getaddrinfo, port_for_getaddrinfo, &hints, &info);
        if (res != 0)
        {
            PRINT("getaddrinfo: %s\n", gai_strerror(res));
            return false;
        }

        for(auto p = info; p != nullptr; p = p->ai_next)
        {
            m_socket = Raw_socket(p->ai_family, p->ai_socktype, p->ai_protocol);

            if (!m_socket.valid())
            {
                PRINT("socket creation failed\n");
                continue;
            }

            res = set_options();
            if (res == SOCKET_ERROR)
            {
                PRINT("set_options failed: %d\n", m_socket.get_last_error());
                m_socket.close();
                continue;
            }

            if (bind_to_ip || bind_to_port)
            {
                PRINT("binding client to ip and/or port...\n");

                res = m_socket.bind(p->ai_addr, p->ai_addrlen);
                if (res == SOCKET_ERROR)
                {
                    PRINT("binding failed: %d\n", m_socket.get_last_error());
                    m_socket.close();
                    continue;
                }

                memset(&hints, 0, sizeof hints);
                hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
                hints.ai_socktype = SOCK_STREAM;

                res = ::getaddrinfo(m_server_ip.c_str(), server_port_string.c_str(), &hints, &info2);
                if (res != 0)
                {
                    PRINT("getaddrinfo: %s\n", gai_strerror(res));
                    m_socket.close();
                    freeaddrinfo(info2);
                    info2 = nullptr;
                    continue;
                }

                res = m_socket.connect(info2->ai_addr, info2->ai_addrlen);
                freeaddrinfo(info2);
                info2 = nullptr;

                if (res == SOCKET_ERROR)
                {
                    PRINT("m_socket.connect failed: %d\n", m_socket.get_last_error());
                    m_socket.close();
                    continue;
                }

            }
            else
            {
                res = ::getaddrinfo(m_server_ip.c_str(), server_port_string.c_str(), &hints, &info);
                if (res != 0 )
                {
                    PRINT("getaddrinfo: %s\n", gai_strerror(res));
                    m_socket.close();
                    continue;
                }

                res = m_socket.connect(info->ai_addr, info->ai_addrlen);

                if (res == SOCKET_ERROR)
                {
                    PRINT("m_socket.connect failed: %d\n", m_socket.get_last_error());
                    m_socket.close();
                    continue;
                }
            }

            m_initialized = true;
            break;
        }

        ::freeaddrinfo(info);
        info = nullptr;

        if (!m_socket.valid())
        {
            PRINT("client failed to connect: %d\n", m_socket.get_last_error());
        }

        return m_initialized;
    }

    bool init(void)
    {
        if (m_initialized)
        {
            PRINT("already initialized!\n");
            return true;
        }

        if (m_server)
        {
            return init_server();
        }
        else
        {
            return init_client();
        }
    }



    bool init(std::string ip_address, uint16_t port, std::string client_bind_ip ="", uint16_t client_bind_port = 0)
    {
        if (m_initialized)
        {
            PRINT("already initialized!\n");
            return true;
        }

        m_server_ip = ip_address;
        m_server_port = port;
        m_client_bind_ip = client_bind_ip;
        m_client_bind_port = client_bind_port;

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
            m_initialized = false;
            m_address_valid = false;
        }
    }



    int set_options()
    {
        int res = 0;

        #ifdef _WIN32
        // res = m_socket.setsockop_bool(SO_REUSEADDR, true); // SO_REUSEADDR
        res = m_socket.setsockop_bool(SO_EXCLUSIVEADDRUSE, true);
        #else
        res = m_socket.setsockop_bool(SO_REUSEADDR, true); // SO_REUSEADDR
        #endif

        PRINT("setsockop_bool res: %d\n", res);

        return res;
    }

    int bind()
    {
        int res = m_socket.bind((const sockaddr *)&m_addr, sizeof(m_addr));
        PRINT("bind res: %d\n", res);
        if (res == SOCKET_ERROR)
        {
            PRINT("bind error: %d\n",  m_socket.get_last_error());
            PRINT("server: %d\n", m_server);
        }
        return res;
    }

    int listen(int backlog)
    {
        int res = SOCKET_ERROR;
        if (m_initialized)
        {
            res = m_socket.listen(backlog);
            PRINT("listen res: %d\n",res);
            PRINT("listen m_socket: ");
            m_socket.print();
        }
        else
        {
            PRINT("not initialized\n");
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


    int send(const char* buffer, int length, int flags=0)
    {
        int res = -1;
        if (m_initialized && !m_server && buffer != nullptr && length>0 && length<=INT32_MAX)
        {
            res = m_socket.send(buffer, length, flags);
        }
        return res;
    }

    int recv(char* buffer, int length, int flags=0)
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
    std::string m_client_bind_ip;
    uint16_t m_client_bind_port;
    bool m_address_valid;
    bool m_initialized;
    bool m_server;
};


//allows servers and clients to wait for events using wait_for_events()
class Socket_waiter
{
public:
    Socket_waiter(bool server=false)
        :
        m_server(server),
        m_initialized(false),
        m_socket(Simple_socket(server)),
        m_socket_set({0}),
        m_socket_list(),
        m_messages(),
        m_message_mutex()
        #ifdef _WIN32
        ,m_dummy_socket(INVALID_SOCKET)
        #else
        ,m_largest_fd(-1)
        ,pfd{-1,-1}
        #endif
    {
        FD_ZERO(&m_socket_set); // clear set
    }

    ~Socket_waiter()
    {
        deinit();
    }

    void deinit()
    {
        // remove from list to prevent double close
        remove_socket_from_select(m_socket.get_raw_socket());

        //close dummysocket/pipe:
        #if defined(_WIN32)
        remove_socket_from_select(m_dummy_socket);
        m_dummy_socket.close();
        #else
        remove_socket_from_select(pfd[0]);
        close(pfd[0]);
        close(pfd[1]);
        #endif

        //close remaining raw sockets (clients):
        for(auto& fd: m_socket_list)
        {
            fd.close();
        }

        m_socket_list.clear();
        m_socket.close();
        m_initialized = false;
    }

    bool is_initialized() const{return m_initialized;}

    bool init(std::string ip_address, uint16_t port, std::string client_bind_ip ="", uint16_t client_bind_port = 0, uint8_t number_of_connections=10)
    {
        if(!m_initialized)
        {
            PRINT("%d calling m_socket.init\n", m_server);
            bool valid = m_socket.init(ip_address, port, client_bind_ip, client_bind_port);
            if (valid)
            {
                int res = 0;
                if (m_server)
                {
                    res = m_socket.listen(number_of_connections+1);//+1 for dummy socket or pipe
                }

                if (res != SOCKET_ERROR)
                {
                    add_socket_to_select(m_socket.get_raw_socket());

                    #if defined(_WIN32)
                    m_dummy_socket = Raw_socket(AF_INET, SOCK_STREAM, 0);
                    if (m_dummy_socket.valid())
                    {
                        PRINT("dummy socket: ");
                        m_dummy_socket.print();
                        add_socket_to_select(m_dummy_socket);
                        m_initialized = true;
                    }
                    #else
                    res = pipe(pfd);
                    PRINT("pipe(pfd): %d\n", res);

                    if(res != SOCKET_ERROR)
                    {
                        add_socket_to_select(pfd[0]);
                    }

                    // Make read and write ends of pipe nonblocking

                    // Make read end nonblocking
                    if(res != SOCKET_ERROR)
                    {
                        res = fcntl(pfd[0], F_GETFL);
                    }
                    if(res != SOCKET_ERROR)
                    {
                        res = fcntl(pfd[0], F_SETFL, res | O_NONBLOCK);
                    }

                    // Make write end nonblocking
                    if(res != SOCKET_ERROR)
                    {
                        res = fcntl(pfd[1], F_GETFL);
                    }
                    if(res != SOCKET_ERROR)
                    {
                        res = fcntl(pfd[1], F_SETFL, res | O_NONBLOCK);
                    }

                    m_initialized = res != SOCKET_ERROR;
                    #endif
                }
            }
        }
        return m_initialized;
    }

    void add_socket_to_select(Raw_socket socket)
    {
        FD_SET(socket.get_native(), &m_socket_set);
        m_socket_list.push_back(socket);
        #ifndef _WIN32
        if (socket.get_native() > m_largest_fd)
        {
            m_largest_fd = socket.get_native();
        }
        #endif
    }

    void remove_socket_from_select(Raw_socket socket)
    {
        FD_CLR(socket.get_native(), &m_socket_set);
        m_socket_list.remove(socket);

        #ifndef _WIN32
        //if largest has been removed a new one needs to be calculated
        if (socket.get_native() == m_largest_fd)
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

    enum class Event_code
    {
        no_event=0,
        not_initialized,
        client_connected,
        client_disconnected,
        client_error,
        rx,
        exit,
        interrupt ,
        timeout
    };

    static const char* Event_code_to_string(Event_code e)
    {
        const char* s = "";

        switch(e)
        {
            case Event_code::no_event:
            s = "no_event";
            break;
            case Event_code::not_initialized:
            s = "not_initialized";
            break;
            case Event_code::client_connected:
            s = "client_connected";
            break;
            case Event_code::client_disconnected:
            s = "client_disconnected";
            break;
            case Event_code::client_error:
            s = "client_error";
            break;
            case Event_code::rx:
            s = "rx";
            break;
            case Event_code::exit:
            s = "exit";
            break;
            case Event_code::interrupt:
            s = "interrupt";
            break;
            case Event_code::timeout:
            s = "timeout";
            break;
        }

        return s;
    }



    struct Event
    {
        Event_code event_code;
        Raw_socket client_socket = INVALID_SOCKET;
        int bytes_available = 0;
        const void* message = nullptr;
        #ifdef SIMPLE_CPP_SOCKETS_CLIENT_ADDRESS
        std::string client_ip_str="";
        uint32_t client_ip4=0;
        uint16_t client_port=0;
        #endif

        std::string to_string() const
        {
            std::string s = "(0)";
            s[1] += char(event_code);
            s += Event_code_to_string(event_code);
            return s;
        }
    };

    void exit_message()
    {
        custom_message(nullptr);
    }

    void custom_message(const void* message)
    {
        std::lock_guard<std::mutex> guard(m_message_mutex);

        if (m_initialized)
        {
            PRINT("adding message\n");

            if (m_messages.size() == 0) // only trigger server once per batch
            {
                #if defined(_WIN32)
                PRINT("closing dummy socket copy\n");
                auto m_dummy_socket_copy = m_dummy_socket;
                m_dummy_socket_copy.close();
                #else
                PRINT("write data to pipe\n");
                [[maybe_unused]] int res = write(pfd[1], "x", 1);
                PRINT("write rs: %d\n", res);
                #endif
            }

            m_messages.push_back(message);
        }
    }

    std::vector<Event> wait_for_events(int32_t timeout_ms = -1)
    {
        std::vector<Event> events;

        if (m_initialized)
        {
            fd_set event_read_set = m_socket_set; //select modifies set, so make a copy
            fd_set event_except_set = m_socket_set; //select modifies set, so make a copy

            #if defined(_WIN32)
                const int nfds = 0; // don't care
            #else
                const int nfds = m_largest_fd+1;
            #endif

            struct timeval* p_timeout = nullptr;

            struct timeval timeout;
            if(timeout_ms>=0)
            {
                timeout.tv_sec  = timeout_ms / 1000;
                timeout.tv_usec = (timeout_ms % 1000) * 1000;
                p_timeout = &timeout;
            }

            //PRINT("select\n");
            int res = select(nfds, &event_read_set, NULL, &event_except_set, p_timeout);

            PRINT("select res: %d\n",res);
            if(res == SOCKET_ERROR)
            {
                [[maybe_unused]] int error = m_socket.get_raw_socket().get_last_error();
                PRINT("select error: %d\n", error);

                //closing dummy socket before select causes error

                #if defined(_WIN32)
                    if(error == 10038) // WSAENOTSOCK
                    {
                        PRINT("WSAENOTSOCK\n");
                        if (m_messages.size() != 0)
                        {
                            PRINT("Dummy socket was probably closed before select\n");
                        }
                    }
                #endif

                FD_ZERO(&event_read_set); // clear set, because it can generate a false event for server socket, which causes accept() to freeze
            }

            // read messages even if select res==SOCKET_ERROR, because closing dummy socket before select causes errors
            {
                std::lock_guard<std::mutex> guard(m_message_mutex);
                if (m_messages.size() != 0)
                {
                    #if defined(_WIN32)
                    PRINT("m_dummy_socket event\n");

                    //remove dummy socket:
                    remove_socket_from_select(m_dummy_socket);
                    m_dummy_socket.mark_as_closed(); //do not close as it was already closed

                    //add a new dummy socket back:
                    m_dummy_socket = Raw_socket(AF_INET, SOCK_STREAM, 0);
                    PRINT("new dummy socket: ");
                    m_dummy_socket.print();
                    add_socket_to_select(m_dummy_socket);
                    #else
                    PRINT("self pipe event\n");
                    char buffer[1+1]="";
                    ::read(pfd[0],buffer, sizeof(buffer)-1);
                    PRINT("received: %s\n", buffer);
                    #endif

                    for(const auto& message: m_messages)
                    {
                        if (message == nullptr)
                        {
                            PRINT("received exit message\n");
                            Event event = {Event_code::exit};
                            events.push_back(event);
                        }
                        else
                        {
                            PRINT("received custom message\n");
                            Event event = {Event_code::interrupt, INVALID_SOCKET, 0, message};
                            events.push_back(event);
                        }
                    }
                    m_messages.clear();
                }
            }

            if(res == SOCKET_ERROR)
            {
                // return because there is nothing to check in event_read_set
                return events;
            }

            bool read_set_event = false;
            auto m_socket_list_copy = m_socket_list;
            for(const auto& raw_socket: m_socket_list_copy)
            {
                PRINT("checking events for: ");
                raw_socket.print();

                if (FD_ISSET(raw_socket.get_native(), &event_except_set)) //new event
                {
                    PRINT("Exception for event for socket: ");
                    raw_socket.print();
                }

                if (FD_ISSET(raw_socket.get_native(), &event_read_set)) //new event
                {
                    read_set_event = true;
                    PRINT("read event for socket: ");
                    raw_socket.print();

                    if (m_server && raw_socket == m_socket.get_raw_socket()) //server event
                    {
                        struct sockaddr_storage remoteaddr; // client address
                        socklen_t addrlen = sizeof(remoteaddr);

                        PRINT("server_event\n");
                        Raw_socket new_socket = m_socket.get_raw_socket().accept((struct sockaddr *)&remoteaddr, &addrlen);
                        // PRINT("accept was called\n");
                        // new_socket.print();

                        if (!new_socket.valid())
                        {
                            int error = m_socket.get_raw_socket().get_last_error();
                            PRINT("accept failed error: %d\n", error);

                            if(error == 10038)
                            {
                                PRINT("WSAENOTSOCK\n");
                            }

                            m_socket.get_raw_socket().mark_as_closed();
                            m_socket.close();

                            Event event= {Event_code::not_initialized};
                            events.push_back(event);
                            return events;

                        }
                        else
                        {
                            PRINT("add_socket_to_select: ");
                            new_socket.print();
                            add_socket_to_select(new_socket);

                            Event event = {Event_code::client_connected, new_socket};

                            #ifdef SIMPLE_CPP_SOCKETS_CLIENT_ADDRESS
                                char remoteIP[INET6_ADDRSTRLEN];
                                const char* address = inet_ntop(remoteaddr.ss_family, get_in_addr(reinterpret_cast<struct sockaddr*>(&remoteaddr)),  remoteIP, INET6_ADDRSTRLEN);
                                uint16_t port = ntohs(get_in_port((struct sockaddr*)&remoteaddr));

                                event.client_ip4 = in_addr_to_uint32(reinterpret_cast<sockaddr*>(&remoteaddr));
                                event.client_ip_str = address;
                                event.client_port = port;
                            #endif

                            events.push_back(event);
                        }
                    }
                    #if defined(_WIN32)
                    else if (raw_socket == m_dummy_socket) //interrupt event
                    {
                        PRINT("m_dummy_socket event\n");
                        //messages already processed before this loop, but keep this condition here to distiquish between client event and interrupt event
                        //alternatively dummy socket or self pipe can be removed from list, but this is less elegant
                    }
                    #else
                    else if (raw_socket == pfd[0]) //interrupt event
                    {
                        PRINT("self pipe event\n");
                        //messages already processed before this loop, but keep this condition here to distiquish between client event and interrupt event
                        //alternatively dummy socket or self pipe can be removed from list, but this is less elegant
                    }
                    #endif
                    else //client event
                    {
                        uint8_t buffer[1024];
                        int peek = raw_socket.recv((char*)buffer, sizeof(buffer), MSG_PEEK);

                        PRINT("peek: %d\n", peek);

                        if (peek < 0)
                        {
                            //error

                            Event event = {Event_code::client_error, raw_socket};
                            events.push_back(event);

                            remove_socket_from_select(raw_socket);

                            if(!m_server)
                            {
                                m_socket.close();
                                m_initialized = false;
                            }
                        }
                        else if (peek == 0)
                        {
                            //close

                            Event event;
                            if(m_server)
                            {
                                event = {Event_code::client_disconnected, raw_socket, 0};
                                remove_socket_from_select(raw_socket);
                            }
                            else
                            {
                                event = {Event_code::client_disconnected, raw_socket, 0}; // server disconnected?
                                remove_socket_from_select(raw_socket);
                                m_socket.close();
                                m_initialized = false;
                            }

                            events.push_back(event);
                        }
                        else
                        {
                            //data
                            Event event = {Event_code::rx, raw_socket, peek};
                            events.push_back(event);
                        }
                    }
                }
            } // for(const auto& raw_socket: m_socket_list_copy)

            if (!read_set_event && res != SOCKET_ERROR && timeout_ms > 0) // assume timeout if no error or event reported by select
            {
                Event event= {Event_code::timeout};
                events.push_back(event);
            }
        }
        else
        {
            Event event= {Event_code::not_initialized};
            events.push_back(event);
        }

        return events;
    }

    void close_client(Raw_socket client)
    {
        remove_socket_from_select(client);
        client.close();
    }

    std::list<Raw_socket> get_client_list()
    {
        std::list<Raw_socket> client_list = m_socket_list;
        client_list.remove(m_socket.get_raw_socket());

        #if defined(_WIN32)
        client_list.remove(m_dummy_socket);
        #else
        client_list.remove(pfd[0]);
        #endif

        return client_list;
    }

    Raw_socket get_raw_socket() const
    {
        return m_socket.get_raw_socket();
    }

    private:

    bool m_server;
    bool m_initialized;
    Simple_socket m_socket;

    fd_set m_socket_set;
    std::list<Raw_socket> m_socket_list;

    std::vector<const void*> m_messages;
    std::mutex m_message_mutex;

    #ifdef _WIN32
    Raw_socket m_dummy_socket;
    #else
    int m_largest_fd;
    int pfd[2]; //both ends of pipe: {read,write}
    #endif

};
