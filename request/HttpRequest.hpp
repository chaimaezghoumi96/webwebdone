#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP
#include <map>
#include <iostream>   // for std::cout, std::endl
#include <string>     // for std::string
#include <sstream>    // for std::stringstream
#include <vector> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <string>
#include "../cookies/Cookie.hpp"
#include <sys/stat.h>  // stat, struct stat, S_ISDIR
#include <unistd.h>    // access, R_OK
#include <errno.h>
#include "../configtypes.hpp"

struct validat {
    std::string path; // "127.0.0.1" or "0.0.0.0" l ip dial server
    int code;         // 1 .. 65535 ..... ex: 8080 port dial server
};

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> cookies;
    std::string body;
    std::map<std::string, std::string> query_params;

    int status; // set to 200, 404, 403, 405, 301...
    std::string redirect_target; // empty unless redirect
    std::string confurm_path; // confirmed/resolved path
    bool is_cgi;
    std::string query_string;
    std::string cgi_script_path;
    std::string cgi_extension;
    std::string cgi_interpreter;
    std::map<std::string, std::string> cgi_env;

    HttpRequest(const std::string& raw_request, const ServerConfig& serv);
    validat validate_request(const ServerConfig& serv);
    bool detect_cgi_request(const ServerConfig& serv);
    void setup_cgi_environment(const ServerConfig& serv, int serverPort);
    void reqq();
};


#endif