#include "response.hpp"
#include <sstream>

std::string intToString(int v)
{
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

Response::Response() : statusCode(200), statusMessage("OK")
{
}

void Response::setStatus(int code, std::string message)
{
    statusCode = code;
    statusMessage = message;
}

void Response::setBody(std::string content)
{
    body = content;
}

void Response::setHeader(std::string key, std::string value)
{
    headers[key] = value;
}
void Response::setCookie(std::string name, std::string value, std::string path, bool httpOnly)
{
    cookieHeaders.push_back(Cookie::buildSetCookie(name, value, path, httpOnly));
}

std::string Response::getBody() const
{
    return body;
}

int Response::getStatusCode() const
{
    return statusCode;
}

std::string Response::buildResponse()
{
    // Content-Length is always recomputed here so callers never have to
    // remember to set it manually before building.
    headers["Content-Length"] = intToString(static_cast<int>(body.size()));

    std::string response;
    response += "HTTP/1.1 ";
    response += intToString(statusCode);
    response += " ";
    response += statusMessage;
    response += "\r\n";

    for (std::map<std::string, std::string>::iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        response += it->first;
        response += ": ";
        response += it->second;
        response += "\r\n";
    }
    for (std::vector<std::string>::iterator it = cookieHeaders.begin();
        it != cookieHeaders.end(); ++it)
    {
        response += "Set-Cookie: ";
        response += *it;
        response += "\r\n";
    }

    response += "\r\n";
    response += body;

    return response;
}

void Response::print() const
{
    std::cout << "===== HTTP RESPONSE =====" << std::endl;
    std::cout << "HTTP/1.1 " << statusCode << " " << statusMessage << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        std::cout << it->first << ": " << it->second << "\r\n";
    }
    std::cout << "Content-Length: " << body.size() << "\r\n";
    std::cout << "\r\n";
    std::cout << body << std::endl;
    std::cout << "=========================" << std::endl;
}
