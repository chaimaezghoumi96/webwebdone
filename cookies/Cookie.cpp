#include "Cookie.hpp"

#include <sstream>

std::string Cookie::trim(const std::string& value)
{
    size_t start = value.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = value.find_last_not_of(" \t");
    return value.substr(start, end - start + 1);
}

Cookie::CookieMap Cookie::parse(const std::string& headerValue)
{
    CookieMap cookies;
    size_t start = 0;

    while (start < headerValue.size())
    {
        size_t separator = headerValue.find(';', start);
        std::string token = headerValue.substr(start,
            separator == std::string::npos ? std::string::npos : separator - start);
        size_t equalPos = token.find('=');

        if (equalPos != std::string::npos)
        {
            std::string name = trim(token.substr(0, equalPos));
            std::string value = trim(token.substr(equalPos + 1));
            if (!name.empty())
                cookies[name] = value;
        }

        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }

    return cookies;
}

std::string Cookie::buildSetCookie(const std::string& name,
                                   const std::string& value,
                                   const std::string& path,
                                   bool httpOnly)
{
    std::ostringstream ss;
    ss << name << "=" << value;
    if (!path.empty())
        ss << "; Path=" << path;
    if (httpOnly)
        ss << "; HttpOnly";
    return ss.str();
}