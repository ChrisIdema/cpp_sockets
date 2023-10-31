#ifdef _WIN32
#include "simple_cpp_sockets2.h"
#include <mutex>
// Number of sockets is tracked to call WSACleanup on Windows
int socket_count;
std::mutex socket_mutex;
WSADATA wsa;
#else
#warning "cpp file not needed in linux"
#endif

