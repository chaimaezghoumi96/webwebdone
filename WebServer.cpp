#include "WebServer.hpp"

#include "socket.hpp"

#include "request/HttpRequest.hpp"

#include <ostream>
#include <sys/socket.h>
#include "response/response.hpp"
#include "response/ResponseHandler.hpp"
#include <netdb.h>
#include <netinet/in.h>

#include <errno.h>
#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <cctype>
#include <sstream>
#include <stdexcept>


static std::string syscallError(const std::string& what)
{
    return what + ": " + ::strerror(errno);
}

static std::string toLower(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

static std::string trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
        ++start;
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        --end;
    return s.substr(start, end - start);
}

static std::string extractHostHeader(const std::string& raw)
{
    std::istringstream ss(raw);
    std::string line;
    bool first = true;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (first) {
            first = false;
            continue;
        }
        if (line.empty())
            break;
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string key = toLower(trim(line.substr(0, colon)));
        if (key != "host")
            continue;
        std::string value = trim(line.substr(colon + 1));
        if (!value.empty() && value[0] == '[') {
            size_t end = value.find(']');
            if (end != std::string::npos)
                return value.substr(1, end - 1);
            return value;
        }
        size_t port = value.find(':');
        if (port != std::string::npos)
            value = value.substr(0, port);
        return value;
    }
    return "";
}

size_t WebServer::selectServerIndex(int listenerFd, const std::string& hostHeader) const
{
    std::map<int, std::vector<size_t> >::const_iterator it = _listenerToServerIndices.find(listenerFd);
    if (it == _listenerToServerIndices.end() || it->second.empty())
        return 0;

    if (hostHeader.empty())
        return it->second[0];

    const std::string host = toLower(hostHeader);
    for (size_t i = 0; i < it->second.size(); ++i) {
        const ServerConfig& sc = _conf.servers[it->second[i]];
        if (!sc.server_name.empty() && toLower(sc.server_name) == host)
            return it->second[i];
    }
    return it->second[0];
}


void WebServer::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw std::runtime_error(syscallError("fcntl(F_GETFL)"));

    if ((flags & O_NONBLOCK) != 0)
        return;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error(syscallError("fcntl(F_SETFL)"));
}

int WebServer::listenerPort(int listenerFd) const
{
    for (size_t i = 0; i < _listeners.size(); ++i)
    {
        if (_listeners[i]->get_fd() == listenerFd)
            return ntohs(_listeners[i]->get_address().sin_port);
    }
    return -1;
}


WebServer::WebServer(const Config& conf)
    : _conf(conf)
{
    typedef std::pair<in_addr_t, int> ListenKey;
    std::map<ListenKey, int>          keyToFd;

    for (size_t sidx = 0; sidx < conf.servers.size(); ++sidx)
    {
        const ServerConfig& sc = conf.servers[sidx];

        for (size_t lidx = 0; lidx < sc.listens.size(); ++lidx)
        {
            const ServerConfig::Listen& l = sc.listens[lidx];

            const in_addr_t addr    = Socket::resolve(l.host);
            const ListenKey key(addr, l.port);

            std::map<ListenKey, int>::iterator it = keyToFd.find(key);
            if (it != keyToFd.end())
            {
                _listenerToServerIndices[it->second].push_back(sidx);
                continue;
            }

            Socket*   s  = new Socket(addr, l.port);
            const int fd = s->get_fd();

            _listeners.push_back(s);
            setNonBlocking(fd);
            addListener(fd);
            keyToFd[key] = fd;
            _listenerToServerIndices[fd].push_back(sidx);
        }
    }
        //print the _listenerToServerIndices
    for (std::map<int, std::vector<size_t> >::const_iterator it = _listenerToServerIndices.begin(); it != _listenerToServerIndices.end(); ++it) {
        std::cout << "Listener fd: " << it->first << " is associated with server indices: ";
        for (size_t i = 0; i < it->second.size(); ++i) {
            std::cout << it->second[i] << " ";
        }
        std::cout << std::endl;
    }
}

WebServer::~WebServer()
{
    for (std::map<int, ClientState>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        (void)close(it->first);
    // Socket objects own listener fds — delete them so they close their fds.
    for (Sockets::iterator it = _listeners.begin(); it != _listeners.end(); ++it) {
        delete *it;
    }

    _listeners.clear();
}

void WebServer::addListener(int listenerFd)
{
    ::pollfd p;
    p.fd = listenerFd;
    p.events = POLLIN;
    p.revents = 0;
    _pollfds.push_back(p);
}

void WebServer::addClient(int clientFd, int listenerFd, size_t serverIndex)
{
    pollfd p;
    p.fd = clientFd;
    p.events = POLLIN;
    p.revents = 0;
    _pollfds.push_back(p);

    _clients[clientFd] = ClientState(listenerFd, serverIndex);
}

void WebServer::closeAndRemove(size_t pollIndex)
{
    const int fd = _pollfds[pollIndex].fd;

    if (!isListenerFd(fd)) {
        (void)close(fd);
        _clients.erase(fd);
    }

    // swap-remove
    if (pollIndex + 1 != _pollfds.size())
        _pollfds[pollIndex] = _pollfds[_pollfds.size() - 1];
    _pollfds.pop_back();
}

void WebServer::handleListenerReadable(int listenerPollIndex)
{
    const int listenerFd = _pollfds[listenerPollIndex].fd;

    std::map<int, std::vector<size_t> >::const_iterator it = _listenerToServerIndices.find(listenerFd);
    if (it == _listenerToServerIndices.end() || it->second.empty())
        throw std::runtime_error("Internal error: listener has no associated server");

    // Default routing for new connections: first configured server{} for this listener.
    const size_t serverIndex = it->second[0];

    while (true) {
        const int clientFd = accept(listenerFd, 0, 0);
        if (clientFd < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            throw std::runtime_error(syscallError("accept"));
        }

        setNonBlocking(clientFd);
        addClient(clientFd, listenerFd, serverIndex);
    }
}

bool WebServer::hasHeaderTerminator(const std::string& s)
{
    return s.find("\r\n\r\n") != std::string::npos || s.find("\n\n") != std::string::npos;
}


static bool headerContainsChunked(const std::string& headers)
{
    std::string lower = headers;
    for (size_t i = 0; i < lower.size(); ++i)
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));

    const size_t pos = lower.find("transfer-encoding:");
    if (pos == std::string::npos)
        return false;

    const size_t lineEnd = lower.find("\r\n", pos);
    const std::string value = lower.substr(pos, lineEnd - pos);
    return value.find("chunked") != std::string::npos;
}

static long getContentLength(const std::string& headers)
{
    std::string lower = headers;
    for (size_t i = 0; i < lower.size(); ++i)
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));

    const size_t pos = lower.find("content-length:");
    if (pos == std::string::npos)
        return -1;

    size_t valStart = pos + std::strlen("content-length:");
    while (valStart < lower.size() && lower[valStart] == ' ')
        ++valStart;

    const size_t valEnd = lower.find("\r\n", valStart);
    const std::string valStr = lower.substr(valStart, valEnd - valStart);

    char* endPtr = NULL;
    const long val = strtol(valStr.c_str(), &endPtr, 10);
    if (endPtr == valStr.c_str())
        return -1;
    return val;
}

static bool isRequestComplete(const std::string& in)
{
    const size_t headerEnd = in.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return false;

    const size_t bodyStart = headerEnd + 4;
    const std::string headers = in.substr(0, headerEnd);


    if (headerContainsChunked(headers))
    {

        const size_t termPos = in.find("0\r\n\r\n", bodyStart);
        return termPos != std::string::npos;
    }

    long contentLength = getContentLength(headers);

    if (contentLength <= 0)
        return true; 

    const size_t bodyReceived = in.size() - bodyStart;
    return bodyReceived >= static_cast<size_t>(contentLength);
}
bool WebServer::handleClientEvents(size_t clientPollIndex)
{
    const int fd = _pollfds[clientPollIndex].fd;

    std::map<int, ClientState>::iterator it = _clients.find(fd);
    if (it == _clients.end()) {
        closeAndRemove(clientPollIndex);
        return true;
    }

    ClientState& st = it->second;
    const short re = _pollfds[clientPollIndex].revents;

    if ((re & POLLIN) != 0) {
        char buf[4096];
        while (true) {
            const ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n > 0) {
                st.in.append(buf, static_cast<size_t>(n));
                continue;
            }
            if (n == 0) {
                closeAndRemove(clientPollIndex);
                return true;
            }

            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            closeAndRemove(clientPollIndex);
            return true;
        }

        if (!st.responded && hasHeaderTerminator(st.in) && isRequestComplete(st.in))
        {
            try {
                // std::cout << "Received request to server: " << st.serverIndex << " on fd: " << st.listenerFd << std::endl;
                // std::cout << std::endl;
                const std::string host = extractHostHeader(st.in);
                const size_t idx = selectServerIndex(st.listenerFd, host);

                std::cout << st.in << std::endl;

                HttpRequest req(st.in, _conf.servers[idx]);

                Response res = ResponseHandler(req, _conf.servers[idx]).handle();
                // res.print();
                // std::cout << std::endl;
                st.out = res.buildResponse();
                // std::cout << st.out << std::endl;
                

            } catch (const std::exception& e) {
                std::cerr << "[webserv] parse error: " << e.what() << std::endl;
            }
            st.responded = true;
        }
    }
// std::cout <<"hello\n";
    if ((re & POLLOUT) != 0 && !st.out.empty()) {
        int sendFlags = 0;
#ifdef MSG_NOSIGNAL
        sendFlags = MSG_NOSIGNAL;
#endif
        while (!st.out.empty()) {
            const ssize_t n = send(fd, st.out.data(), st.out.size(), sendFlags);
            if (n > 0) {
                st.out.erase(0, static_cast<size_t>(n));
                continue;
            }

            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;

            closeAndRemove(clientPollIndex);
            return true;
        }

        if (st.responded && st.out.empty()) {
            closeAndRemove(clientPollIndex);
            return true;
        }
    }

    return false;
}

void WebServer::run()
{
    if (_pollfds.empty())
        throw std::runtime_error("No listen sockets configured");

    while (true) {
        for (size_t i = 0; i < _pollfds.size(); ++i) {
            const int fd = _pollfds[i].fd;
            _pollfds[i].revents = 0;

            if (isListenerFd(fd)) {
                _pollfds[i].events = POLLIN;
                continue;
            }

            short ev = POLLIN;
            std::map<int, ClientState>::const_iterator it = _clients.find(fd);
            if (it != _clients.end() && !it->second.out.empty())
                ev |= POLLOUT;
            _pollfds[i].events = ev;
        }

        const int rc = poll(&_pollfds[0], _pollfds.size(), -1);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error(syscallError("poll"));
        }

        for (size_t i = 0; i < _pollfds.size(); )
        {
            if (_pollfds[i].revents == 0) {
                ++i;
                continue;
            }

            if ((_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
            {
                if (isListenerFd(_pollfds[i].fd))
                    throw std::runtime_error("Listener socket failed");
                closeAndRemove(i);
                continue;
            }

            if (isListenerFd(_pollfds[i].fd))
            {
                if ((_pollfds[i].revents & POLLIN) != 0)
                    handleListenerReadable(static_cast<int>(i));
                ++i;
                continue;
            }

            if (handleClientEvents(i))
                continue;

            ++i;
        }
    }
}

bool WebServer::isListenerFd(int fd) const
{
    for (size_t i = 0; i < _listeners.size(); ++i)
        if (_listeners[i]->get_fd() == fd)
            return true;
    return false;
}

