#include <iostream>
#include "configtypes.hpp"
#include "configloader.hpp"
#include "WebServer.hpp"


void display_data(const Config& conf) {
    std::cout << "Config loaded successfully:\n";
    for (size_t i = 0; i < conf.servers.size(); ++i) {
        const ServerConfig& s = conf.servers[i];
        std::cout << "Server " << i << ":\n";
        std::cout << "  Listen:\n";
        for (size_t j = 0; j < s.listens.size(); ++j) {
            std::cout << "    - " << s.listens[j].host << ":" << s.listens[j].port << "\n";
        }
        std::cout << "  Server Name: " << (s.server_name.empty() ? "(none)" : s.server_name) << "\n";
        std::cout << "  Root: " << s.root << "\n";
        std::cout << "  Client Max Body Size: " << s.client_max_body_size << "\n";
        std::cout << "  Error Pages:\n";
        for (std::map<int, std::string>::const_iterator it = s.error_pages.begin(); it != s.error_pages.end(); ++it) {
            std::cout << "    - " << it->first << ": " << it->second << "\n";
        }
        //location data displayed here
        std::cout << "  Locations:\n";
        for (size_t k = 0; k < s.locations.size(); ++k) {
            const ServerConfig::LocationConfig& loc = s.locations[k];
            std::cout << "    Location " << k << ":\n";
            std::cout << "      Prefix: " << loc.prefix << "\n";
            std::cout << "      Methods: ";
            if (loc.methods.empty()) {
                std::cout << "(default)";
            } else {
                for (ServerConfig::LocationConfig::MethodSet::const_iterator it = loc.methods.begin(); it != loc.methods.end(); ++it) {
                    std::cout << *it << " ";
                }
            }
            std::cout << "\n";
            if (loc.redirect.enabled) {
                std::cout << "      Redirect: " << loc.redirect.code << " to " << loc.redirect.target << "\n";
            }
            if (!loc.root.empty()) {
                std::cout << "      Root: " << loc.root << "\n";
            }
            std::cout << "      Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
            if (!loc.index.empty()) {
                std::cout << "      Index files: ";
                for (size_t m = 0; m < loc.index.size(); ++m) {
                    std::cout << loc.index[m] << " ";
                }
                std::cout << "\n";
            }
            if (loc.upload.enabled) {
                std::cout << "      Upload dir: " << loc.upload.dir << "\n";
            }
            if (!loc.cgi.empty()) {
                std::cout << "      CGI:\n";
                for (ServerConfig::LocationConfig::CgiMap::const_iterator it = loc.cgi.begin(); it != loc.cgi.end(); ++it) {
                    std::cout << "        - Extension: " << it->first << ", Executable: " << it->second << "\n";
                }
            }
        }
    }
}


/*    std::cout << "Listener sockets created:\n";
    for (std::map<ListenKey, int>::const_iterator it = keyToFd.begin(); it != keyToFd.end(); ++it) {
        const ListenKey& key = it->first;
        const int fd = it->second;
        struct in_addr inAddr;
        inAddr.s_addr = key.first;
        //manual spliting without using inet_ntoa
        unsigned char* bytes = (unsigned char*)&inAddr.s_addr;
        std::cout << "  " << (int)bytes[0] << "." << (int)bytes[1] << "." << (int)bytes[2] << "." << (int)bytes[3] << ":" << key.second << " -> fd " << fd << std::endl;
    }*/
int main (int argc, char ** argv)
{
    if (argc != 2) {
        std::cerr << "Error: invalid number of parameters\n Usage: ./server [config.conf]" << std::endl;
        return 1;
    }

    Config conf;
    std::string errorMessage;
    if (!ConfigLoader().tryLoadFromFile(argv[1], conf, &errorMessage)) {
        std::cerr << "Error: failed to load config file: " << argv[1] << ": " << errorMessage << std::endl;
        return 1;
    }
    // display_data(conf);
    try {
        WebServer server(conf);
        server.run ();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
