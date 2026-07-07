#ifndef RESPONSEHANDLER_HPP
# define RESPONSEHANDLER_HPP

#include <string>
#include "response.hpp"
#include "../request/HttpRequest.hpp"
#include "../configtypes.hpp"

// Resolves an HttpRequest (already validated against a ServerConfig) into
// a Response, dispatching on req.status / req.method.
class ResponseHandler
{
    public:
        ResponseHandler(const HttpRequest& r, const ServerConfig& c);

        // Entry point: inspects req.status and req.method and dispatches
        // to the appropriate handler below.
        Response handle();

    private:
        const HttpRequest&  req;
        const ServerConfig& conf;

        Response handleGET(const std::string& path);
        Response handleDELETE();
        Response handlePOST();
        Response handleCGI();
        Response handleAutoIndex(const std::string& path);
        Response handleReqErrors();

        // Not assignable: holds references.
        ResponseHandler& operator=(const ResponseHandler& other);
};

// --- Free helpers used by ResponseHandler.cpp (also declared here so other
//     translation units can reuse them if needed) ---
bool        readFile(const std::string& path, std::string& content);
std::string getMimeType(const std::string& path);
std::string getExtFromContentType(const std::string& ct);
std::string toString(int n);

#endif
