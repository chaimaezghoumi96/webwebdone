#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include "../cookies/Cookie.hpp"

// Converts an int to a std::string without C++11 std::to_string.
std::string intToString(int v);

class Response
{
    public:
        Response();

        void setStatus(int code, std::string message);
        void setBody(std::string content);
        void setHeader(std::string key, std::string value);
        void setCookie(std::string name, std::string value, std::string path, bool httpOnly);

        std::string getBody() const;
        int getStatusCode() const;

        // Builds the raw HTTP/1.1 response string (status line + headers + body).
        std::string buildResponse();

        // Debug helper: prints the response to stdout.
        void print() const;

    private:
        int                                statusCode;
        std::string                        statusMessage;
        std::string                        body;
        std::map<std::string, std::string> headers;
        std::vector<std::string>           cookieHeaders;
};

#endif
