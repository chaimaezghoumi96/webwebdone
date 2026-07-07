#ifndef COOKIE_HPP
#define COOKIE_HPP

#include <map>
#include <string>

class Cookie
{
    public:
        typedef std::map<std::string, std::string> CookieMap;

        static CookieMap parse(const std::string& headerValue);
        static std::string buildSetCookie(const std::string& name,
                                          const std::string& value,
                                          const std::string& path,
                                          bool httpOnly);

    private:
        static std::string trim(const std::string& value);
};

#endif