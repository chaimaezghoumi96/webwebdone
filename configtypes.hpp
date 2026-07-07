#ifndef CONFIGTYPES_HPP
#define CONFIGTYPES_HPP

#include <string>
// #include <iostream>
#include <vector>
#include <map>
#include <set>


struct ServerConfig {
    struct Listen {
        std::string host; // "127.0.0.1" or "0.0.0.0" l ip dial server
        int port;         // 1 .. 65535 ..... ex: 8080 port dial server
    };
    std::vector<Listen> listens;                 // interface:port pairs

    std::string server_name;                     // optional
    std::string root;                            // required
    size_t client_max_body_size;                 // bytes; 0 => use default
    std::map<int, std::string> error_pages;      // 404 -> "path"
    struct LocationConfig {
        struct Redirect {
            bool enabled;
            int code;               // 301/302/...
            std::string target;     // "/new" or full URL
            Redirect() : enabled(false), code(0) {}
        };
        struct UploadConfig {
            bool enabled;
            std::string dir;
            UploadConfig() : enabled(false) {}
        };
        typedef std::set<std::string> MethodSet;
        typedef std::map<std::string, std::string> CgiMap; // ".py" -> "/usr/bin/python3" ... 
        
        
        std::string prefix;                 // "/upload/" "/cgi-bin/" etc.
        MethodSet methods;                  // allowed methods; empty => server/default
        Redirect redirect;                  // optional
        std::string root;                   // optional override
        bool autoindex;                     // default false
        std::vector<std::string> index;     // ["index.html", ...]
        UploadConfig upload;                // optional
        CgiMap cgi;                         // optional

        LocationConfig() : autoindex(false) {}
    };

    std::vector<LocationConfig> locations;

    ServerConfig() : client_max_body_size(0) {}
};

struct Config {
    std::vector<ServerConfig> servers;
};

// struct Listen {
//     std::string host; // "127.0.0.1" or "0.0.0.0" l ip dial server
//     int port;         // 1 .. 65535 ..... ex: 8080 port dial server
// };

// struct Redirect {
//     bool enabled;
//     int code;               // 301/302/...
//     std::string target;     // "/new" or full URL
//     Redirect() : enabled(false), code(0) {}
// };

// struct UploadConfig {
//     bool enabled;
//     std::string dir;
//     UploadConfig() : enabled(false) {}
// };

enum TokenType {
    TOK_WORD,     // identifiers + values: listen, 127.0.0.1, /upload/, on, 404, etc.
    TOK_LBRACE,   // {
    TOK_RBRACE,   // }
    TOK_SEMI,     // ;
    TOK_EOF       // end of input (optional but very convenient for parser)
};

struct Token {
    TokenType type;
    std::string text; // only used for TOK_WORD (and maybe TOK_EOF = "")
    int line;

    Token() : type(TOK_EOF), text(""), line(1) {}
    Token(TokenType t, const std::string& s, int ln) : type(t), text(s), line(ln) {}
};
typedef std::vector<Token> TokenList;

// typedef std::set<std::string> MethodSet;
// typedef std::map<std::string, std::string> CgiMap; // ".py" -> "/usr/bin/python3" ...

// struct LocationConfig {
//     std::string prefix;                 // "/upload/" "/cgi-bin/" etc.
//     MethodSet methods;                  // allowed methods; empty => server/default
//     Redirect redirect;                  // optional
//     std::string root;                   // optional override
//     bool autoindex;                     // default false
//     std::vector<std::string> index;     // ["index.html", ...]
//     UploadConfig upload;                // optional
//     CgiMap cgi;                         // optional

//     LocationConfig() : autoindex(false) {}
// };




struct DirectiveProps {
    bool seen;           // have we encountered it (for the current server block)
    bool isMandatory;    // must exist?
    bool allowMultiple;  // can appear more than once?

    DirectiveProps() : seen(false), isMandatory(false), allowMultiple(false) {}
    DirectiveProps(bool s, bool m, bool multi)
        : seen(s), isMandatory(m), allowMultiple(multi) {}
};

typedef std::map<std::string, DirectiveProps> ServerContent;

struct DirectiveInfo {
    ServerContent content;

    DirectiveInfo() {
        content["listen"]               = DirectiveProps(false, true,  true);
        content["server_name"]          = DirectiveProps(false, false, false);
        content["root"]                 = DirectiveProps(false, true,  false); // maybe maybe
        content["client_max_body_size"] = DirectiveProps(false, false, false);
        content["error_page"]           = DirectiveProps(false, false, true);
        content["location"]             = DirectiveProps(false, false, true);
    }
};


//all possible location block content
/*
    prefix ::: mandatory, only one allowed;
        ex: /upload/ /cgi-bin/
    allowed_methods  ::: optional, only one allowed; default empty (use server/default)
        ex: GET POST DELETE
    redirect  ::: optional, only one allowed; default none
        ex: return 301 http://example.com/;
    root    ::: optional, only one allowed; default none (use server/default)
        ex: root /var/www/html/upload/;
    autoindex  ::: optional, only one allowed; default false
        ex: autoindex on;
    index  ::: optional, only one allowed; default empty
        ex: index index.html index.htm;
    upload  ::: optional, only one allowed; default none
        ex: upload /var/www/html/upload/;
    cgi  ::: optional, multiple allowed; default empty
        ex: cgi .php /usr/bin/php-cgi;
    return  ::: optional, only one allowed; default none
        ex: return 301 http://example.com/;
*/



struct LocationProps {
    bool seen;
    bool isMandatory;
    bool allowMultiple;

    LocationProps() : seen(false), isMandatory(false), allowMultiple(false) {}
    LocationProps(bool s, bool m, bool multi)
        : seen(s), isMandatory(m), allowMultiple(multi) {}
};


typedef std::map<std::string, LocationProps> LocationContent;

struct LocationInfo {
    LocationContent content;

    LocationInfo() {
        content["prefix"]          = LocationProps(false, true,  false);
        content["allowed_methods"] = LocationProps(false, false, false);
        content["redirect"]        = LocationProps(false, false, false);
        content["root"]            = LocationProps(false, false, false);
        content["autoindex"]       = LocationProps(false, false, false);
        content["index"]           = LocationProps(false, false, false);
        content["upload"]          = LocationProps(false, false, false);
        content["cgi"]             = LocationProps(false, false, true);
        content["return"]          = LocationProps(false, false, false);
    }
};

#endif