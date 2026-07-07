#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "configtypes.hpp"
#include "socket.hpp"

#include <poll.h>

#include <cstddef>
#include <map>
#include <string>
#include <vector>


class WebServer {
public:
    WebServer(const Config& conf);
    ~WebServer();
    
    // Blocking call: runs the poll() event loop until the process is terminated.
    void run();
    
private:
    const Config& _conf;

    struct ClientState {
        std::string in;
        std::string out;
        int listenerFd;
        size_t serverIndex;
        bool responded;
        
        ClientState() : listenerFd(-1), serverIndex(0), responded(false) {}
        ClientState(int lfd, size_t sidx)
        : listenerFd(lfd), serverIndex(sidx), responded(false) {}
    };

    std::vector< ::pollfd > _pollfds;
    Sockets _listeners;
    std::map<int, std::vector<size_t> > _listenerToServerIndices;
    std::map<int, ClientState> _clients;

    static void setNonBlocking(int fd);
    bool isListenerFd(int fd) const;
    void addListener(int listenerFd);
    void addClient(int clientFd, int listenerFd, size_t serverIndex);
    size_t selectServerIndex(int listenerFd, const std::string& hostHeader) const;

    void closeAndRemove(size_t pollIndex);

    void handleListenerReadable(int listenerPollIndex);
    bool handleClientEvents(size_t clientPollIndex);
    int listenerPort(int listenerFd) const;

    static bool hasHeaderTerminator(const std::string& s);
};

#endif