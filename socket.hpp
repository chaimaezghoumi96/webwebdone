#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <vector>
#include <netinet/in.h>
#include <string>

class Socket {
public:
    Socket(const std::string& host, int port);
    Socket(in_addr_t addr, int port);
    ~Socket();

    int         get_fd()      const;
    sockaddr_in get_address() const;
    in_addr_t   resolvedAddr() const;
    static in_addr_t resolve(const std::string& host);
private:
    int         _fd;
    sockaddr_in _addr;
};

typedef std::vector<Socket*> Sockets;

#endif
