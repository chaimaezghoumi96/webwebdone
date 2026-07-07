#include <string>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include "configtypes.hpp"
#include "configloader.hpp"

// static std::string toLower(const std::string& s)
// {
//     std::string out = s;
//     for (size_t i = 0; i < out.size(); ++i)
//         out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
//     return out;
// }
// static int parseIntRange(const std::string& s, int minV, int maxV)
// {
//     if (s.empty())
//         throw std::runtime_error("empty number");

//     for (size_t i = 0; i < s.size(); ++i) {
//         if (!std::isdigit(static_cast<unsigned char>(s[i])))
//             throw std::runtime_error("non-digit in number: " + s);
//     }

//     char* end = 0;
//     // errno = 0;
//     long v = std::strtol(s.c_str(), &end, 10);
//     if (/*errno == ERANGE|| */ end == s.c_str() || *end != '\0')
//         throw std::runtime_error("bad number: " + s);

//     if (v < minV || v > maxV)
//         throw std::runtime_error("number out of range: " + s);

//     return static_cast<int>(v);
// }

static int parseIntRange(const std::string& s, int minV, int maxV, int line)
{
    if (s.empty())
        throw ParseError("expected a number, got empty string", line);
    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            throw ParseError("expected a number, got: " + s, line);
    char* end;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0')
        throw ParseError("invalid number: " + s, line);
    if (v < minV || v > maxV)
        throw ParseError("number out of range [" + intToStr(minV) + ", "
                         + intToStr(maxV) + "]: " + s, line);
    return static_cast<int>(v);
}

// static bool isValidIPv4(const std::string& ip)
// {
//     int parts = 0;
//     size_t start = 0;

//     while (true) {
//         size_t dot = ip.find('.', start);
//         std::string part = (dot == std::string::npos)
//             ? ip.substr(start)
//             : ip.substr(start, dot - start);

//         if (part.empty()) return false;

//         try {
//             (void)parseIntRange(part, 0, 255, 0);
//         } catch (...) {
//             return false;
//         }

//         ++parts;
//         if (dot == std::string::npos) break;
//         start = dot + 1;
//     }

//     return parts == 4;
// }

// ServerConfig::Listen parseListenIPv4Port4(const std::string& s)
// {
//     size_t colon = s.find(':');
//     if (colon == std::string::npos)
//         throw std::runtime_error("listen: missing ':' (expected ip:port)");

//     if (s.find(':', colon + 1) != std::string::npos)
//         throw std::runtime_error("listen: too many ':' characters");

//     std::string ip = s.substr(0, colon);
//     std::string portStr = s.substr(colon + 1);

//     if (toLower(ip) == "localhost")
//         ip = "127.0.0.1";

//     if (!isValidIPv4(ip))
//         throw std::runtime_error("listen: invalid IPv4 address: " + ip);

//     int port = parseIntRange(portStr, 0, 65535, 0);

//     ServerConfig::Listen l;
//     l.host = ip;
//     l.port = port;
//     return l;
// }

ServerConfig::Listen parseListen(const std::string& s)
{
    // must contain exactly one ':'
    size_t colon = s.find(':');
    if (colon == std::string::npos)
        throw std::runtime_error("listen: missing ':' (expected host:port)");
    if (s.find(':', colon + 1) != std::string::npos)
        throw std::runtime_error("listen: too many ':' characters");

    std::string host = s.substr(0, colon);
    std::string portStr = s.substr(colon + 1);

    if (host.empty())
        throw std::runtime_error("listen: empty host");
    if (portStr.empty())
        throw std::runtime_error("listen: empty port");

    int port = parseIntRange(portStr, 0, 65535, 0);

    ServerConfig::Listen l;
    l.host = host;
    l.port = port;
    return l;
}
