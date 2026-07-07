#include "socket.hpp"

#include <errno.h>
#include <netdb.h>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

int Socket::get_fd() const
{
    return _fd; 
}

sockaddr_in Socket::get_address() const
{
    return _addr;
}

static std::string syscallError(const std::string& what)
{
    return what + ": " + ::strerror(errno);
}

static std::string listenKey(const std::string& host, int port)
{
    std::ostringstream ss;
    ss << host << ":" << port;
    return ss.str();
}

// struct addrinfo
// {
//   int ai_flags;			/* Input flags.  */
//   int ai_family;		/* Protocol family for socket.  */
//   int ai_socktype;		/* Socket type.  */
//   int ai_protocol;		/* Protocol for socket.  */
//   socklen_t ai_addrlen;		/* Length of socket address.  */
//   struct sockaddr *ai_addr;	/* Socket address for socket.  */
//   char *ai_canonname;		/* Canonical name for service location.  */
//   struct addrinfo *ai_next;	/* Pointer to next in list.  */
// };


Socket::Socket(in_addr_t addr, int port)
    : _fd(-1)
{
    if (port < 0 || port > 65535)
        throw std::runtime_error("Invalid listen port");

    memset(&_addr, 0, sizeof(_addr));
    _addr.sin_family      = AF_INET;
    _addr.sin_port        = htons(static_cast<unsigned short>(port));
    _addr.sin_addr.s_addr = addr;

    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd < 0)
        throw std::runtime_error(syscallError("socket"));

    int opt = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(_fd);
        throw std::runtime_error(syscallError("setsockopt(SO_REUSEADDR)"));
    }

    if (bind(_fd, (sockaddr*)&_addr, sizeof(_addr)) < 0) {
        close(_fd);
        throw std::runtime_error(syscallError("bind"));
    }

    if (listen(_fd, 128) < 0) {
        close(_fd);
        throw std::runtime_error(syscallError("listen"));
    }
}

Socket::Socket(const std::string& host, int port)
    : _fd(-1)
{
    if (port < 0 || port > 65535)
        throw std::runtime_error("Invalid listen port");

    struct addrinfo hints;
    struct addrinfo* result = NULL;

    memset(&hints, 0, sizeof(hints));//clear the structure
    hints.ai_family   = AF_INET;//ipv4 only
    hints.ai_socktype = SOCK_STREAM; //tcp socket only

    const int rc = getaddrinfo(host.c_str(), 0, &hints, &result);
    if (rc != 0 || !result || !result->ai_addr || result->ai_family != AF_INET || result->ai_addrlen < (socklen_t)sizeof(sockaddr_in)) {
        if (result)
            freeaddrinfo(result);
        throw std::runtime_error("Invalid listen host: " + host);
    }

    memset(&_addr, 0, sizeof(_addr));//clear the structure
    _addr.sin_family = AF_INET; // ipv
    _addr.sin_port   = htons(static_cast<unsigned short>(port));//
    _addr.sin_addr   = ((struct sockaddr_in*)result->ai_addr)->sin_addr;
    freeaddrinfo(result);

    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd < 0)
        throw std::runtime_error(syscallError("socket"));

    int opt = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(_fd);
        throw std::runtime_error(syscallError("setsockopt(SO_REUSEADDR)"));
    }

    if (bind(_fd, (sockaddr*)&_addr, sizeof(_addr)) < 0) {
        const std::string msg = syscallError("bind(" + listenKey(host, port) + ")");
        close(_fd);
        throw std::runtime_error(msg);
    }

    if (listen(_fd, 128) < 0) {
        const std::string msg = syscallError("listen(" + listenKey(host, port) + ")");
        close(_fd);
        throw std::runtime_error(msg);
    }
}

in_addr_t Socket::resolve(const std::string& host)
{
    struct addrinfo hints;
    struct addrinfo* result = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const int rc = getaddrinfo(host.c_str(), 0, &hints, &result);
    if (rc != 0 || !result || !result->ai_addr
        || result->ai_family != AF_INET
        || result->ai_addrlen < (socklen_t)sizeof(sockaddr_in))
    {
        if (result) freeaddrinfo(result);
        throw std::runtime_error("Invalid listen host: " + host);
    }

    const in_addr_t addr =
        ((struct sockaddr_in*)result->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(result);
    return addr;
}

Socket::~Socket()
{
    if (_fd >= 0)
        close(_fd);
}