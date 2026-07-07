#include "ResponseHandler.hpp"
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <cctype>
#include <vector>

ResponseHandler::ResponseHandler(const HttpRequest& r, const ServerConfig& c)
    : req(r), conf(c)
{
}

std::string toString(int n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

bool readFile(const std::string& path, std::string& content)
{
    std::ifstream file(path.c_str(), std::ios::binary);

    if (!file.is_open())
        return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    content = buffer.str();
    return true;
}

std::string getMimeType(const std::string& path)
{
    size_t pos_dot = path.find_last_of('.');

    if (pos_dot == std::string::npos)
        return "text/plain";

    std::string ext = path.substr(pos_dot);
    if (ext == ".html" || ext == ".htm")
        return "text/html";
    if (ext == ".css")
        return "text/css";
    if (ext == ".js")
        return "application/javascript";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".txt")
        return "text/plain";
    if (ext == ".pdf")
        return "application/pdf";
    if (ext == ".json")
        return "application/json";

    return "application/octet-stream";
}

std::string getExtFromContentType(const std::string& ct)
{
    if (ct.find("text/plain") != std::string::npos ||
        ct.find("application/x-www-form-urlencoded") != std::string::npos)
        return ".txt";
    if (ct.find("text/html") != std::string::npos)
        return ".html";
    if (ct.find("application/json") != std::string::npos)
        return ".json";
    if (ct.find("image/png") != std::string::npos)
        return ".png";
    if (ct.find("image/jpeg") != std::string::npos || ct.find("image/jpg") != std::string::npos)
        return ".jpg";
    return ".bin";
}

// Joins a directory and a filename, inserting exactly one '/' between them.
static std::string joinPath(const std::string& dir, const std::string& name)
{
    if (dir.empty())
        return name;
    if (dir[dir.size() - 1] == '/')
        return dir + name;
    return dir + "/" + name;
}

// Strips surrounding whitespace/quotes and any trailing ";..." parameters
// from a raw header value fragment (e.g. from Content-Disposition).
static std::string trimUploadName(const std::string& name)
{
    std::string::size_type start = name.find_first_not_of(" \t\r\n\f\v\"");
    if (start == std::string::npos)
        return "";

    std::string::size_type end = name.find_last_not_of(" \t\r\n\f\v\"");
    std::string value = name.substr(start, end - start + 1);

    std::string::size_type semi = value.find(';');
    if (semi != std::string::npos)
        value = value.substr(0, semi);

    return value;
}

// Strips any directory components a (possibly hostile) client might have
// included in a filename, keeping only the final path segment.
static std::string getBasename(const std::string& name)
{
    std::string cleaned = trimUploadName(name);
    std::string::size_type slash_pos = cleaned.find_last_of("/\\");
    if (slash_pos == std::string::npos)
        return cleaned;
    return cleaned.substr(slash_pos + 1);
}

// Returns the original filename the client sent (via X-File-Name or
// Content-Disposition), or "" if none was provided.
static std::string getOriginalFilename(const HttpRequest& req)
{
    std::map<std::string, std::string>::const_iterator it = req.headers.find("X-File-Name");
    if (it != req.headers.end() && !it->second.empty())
        return getBasename(it->second);

    it = req.headers.find("Content-Disposition");
    if (it != req.headers.end())
    {
        std::string::size_type pos = it->second.find("filename=");
        if (pos != std::string::npos)
        {
            std::string filename = it->second.substr(pos + 9);
            if (!filename.empty() && filename[0] == '"')
            {
                std::string::size_type end = filename.find('"', 1);
                if (end != std::string::npos)
                    filename = filename.substr(1, end - 1);
            }
            filename = getBasename(filename);
            if (!filename.empty())
                return filename;
        }
    }

    return "";
}

// Prefers the extension from the client's original filename (accurate).
// Falls back to Content-Type sniffing only if no filename was supplied.
static std::string resolveUploadExt(const HttpRequest& req, const std::string& originalName)
{
    if (!originalName.empty())
    {
        std::string::size_type dot_pos = originalName.find_last_of('.');
        if (dot_pos != std::string::npos && dot_pos != 0)
            return originalName.substr(dot_pos);
    }

    std::map<std::string, std::string>::const_iterator it = req.headers.find("Content-Type");
    if (it != req.headers.end())
        return getExtFromContentType(it->second);

    return ".bin";
}

// Appends "storedName -> originalName" to uploads.log in the target
// directory, so the mapping survives without adding extra files that
// would clutter autoindex or be independently GET/DELETE-able.
static void logUploadMapping(const std::string& dir,
                              const std::string& storedName,
                              const std::string& originalName)
{
    if (originalName.empty())
        return;

    std::string logPath = joinPath(dir, "uploads.log");
    std::ofstream log(logPath.c_str(), std::ios::app);
    if (log.is_open())
        log << storedName << " -> " << originalName << "\n";
}

// RFC 1123 date format required for HTTP Date headers, e.g.
// "Wed, 01 Jul 2026 12:00:00 GMT".
static std::string getHttpDate()
{
    time_t now = time(NULL);
    struct tm* gmt = gmtime(&now);
    char buf[64];

    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buf);
}

static std::string trimCgiToken(const std::string& value)
{
    std::string::size_type start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    std::string::size_type end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

static std::string lowerString(std::string value)
{
    for (std::string::size_type i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string getDefaultStatusMessage(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static bool parseCgiResponse(const std::string& raw, Response& res)
{
    if (raw.empty())
        return false;

    std::string::size_type header_end = raw.find("\r\n\r\n");
    std::string body;
    std::string headers_block;
    if (header_end != std::string::npos)
    {
        headers_block = raw.substr(0, header_end);
        body = raw.substr(header_end + 4);
    }
    else
    {
        header_end = raw.find("\n\n");
        if (header_end != std::string::npos)
        {
            headers_block = raw.substr(0, header_end);
            body = raw.substr(header_end + 2);
        }
        else
        {
            body = raw;
            headers_block.clear();
        }
    }

    int status_code = 200;
    std::string status_message = getDefaultStatusMessage(status_code);
    bool has_content_type = false;

    std::stringstream header_stream(headers_block);
    std::string line;
    while (std::getline(header_stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        line = trimCgiToken(line);
        if (line.empty())
            continue;

        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = trimCgiToken(line.substr(0, colon));
        std::string value = trimCgiToken(line.substr(colon + 1));
        std::string lower_key = lowerString(key);

        if (lower_key == "status")
        {
            std::istringstream iss(value);
            iss >> status_code;
            if (!iss.fail())
            {
                std::string rest;
                std::getline(iss, rest);
                rest = trimCgiToken(rest);
                if (!rest.empty())
                    status_message = rest;
                else
                    status_message = getDefaultStatusMessage(status_code);
            }
            continue;
        }

        if (lower_key == "content-type")
            has_content_type = true;

        res.setHeader(key, value);
    }

    if (!has_content_type)
        res.setHeader("Content-Type", "text/plain");

    res.setStatus(status_code, status_message);
    res.setBody(body);
    return true;
}

static bool runCgiProgram(const HttpRequest& req, std::string& raw_output)
{
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0)
        return false;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return false;
    }

    if (pid == 0)
    {
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        std::vector<std::string> env_store;
        env_store.reserve(req.cgi_env.size());
        std::vector<char*> envp;
        envp.reserve(req.cgi_env.size() + 1);
        for (std::map<std::string, std::string>::const_iterator it = req.cgi_env.begin();
             it != req.cgi_env.end(); ++it)
        {
            env_store.push_back(it->first + "=" + it->second);
            envp.push_back(const_cast<char*>(env_store.back().c_str()));
        }
        envp.push_back(NULL);

        char* argv[3];
        argv[0] = const_cast<char*>(req.cgi_interpreter.c_str());
        argv[1] = const_cast<char*>(req.cgi_script_path.c_str());
        argv[2] = NULL;

        execve(req.cgi_interpreter.c_str(), argv, &envp[0]);
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    const char* input = req.body.data();
    size_t remaining = req.body.size();
    while (remaining > 0)
    {
        ssize_t written = write(stdin_pipe[1], input, remaining);
        if (written < 0)
        {
            if (errno == EINTR)
                continue;
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            waitpid(pid, NULL, 0);
            return false;
        }
        input += written;
        remaining -= static_cast<size_t>(written);
    }
    close(stdin_pipe[1]);

    char buffer[4096];
    while (true)
    {
        ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
        if (n > 0)
        {
            raw_output.append(buffer, static_cast<size_t>(n));
            continue;
        }
        if (n == 0)
            break;
        if (errno == EINTR)
            continue;
        close(stdout_pipe[0]);
        waitpid(pid, NULL, 0);
        return false;
    }
    close(stdout_pipe[0]);

    int child_status = 0;
    if (waitpid(pid, &child_status, 0) < 0)
        return false;
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0)
        return false;

    return true;
}

Response ResponseHandler::handleCGI()
{
    Response res;
    std::string raw_output;

    res.setHeader("Date", getHttpDate());

    if (!runCgiProgram(req, raw_output))
    {
        res.setStatus(500, "Internal Server Error");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>500 CGI Execution Failed</h1>");
        return res;
    }

    if (!parseCgiResponse(raw_output, res))
    {
        res.setStatus(500, "Internal Server Error");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>500 Invalid CGI Response</h1>");
        return res;
    }

    return res;
}

Response ResponseHandler::handleReqErrors()
{
    Response res;

    if (req.status == 301 && !req.redirect_target.empty())
    {
        res.setStatus(301, "Moved Permanently");
        res.setHeader("Location", req.redirect_target);
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>301 Moved Permanently</h1>");
    }
    else if (req.status == 400)
    {
        res.setStatus(400, "Bad Request");
        res.setBody("<h1>400 Bad Request</h1>");
    }
    else if (req.status == 403)
    {
        res.setStatus(403, "Forbidden");
        res.setBody("<h1>403 Forbidden</h1>");
    }
    else if (req.status == 404)
    {
        res.setStatus(404, "Not Found");
        res.setBody("<h1>404 Not Found</h1>");
    }
    else if (req.status == 405)
    {
        res.setStatus(405, "Method Not Allowed");
        res.setBody("<h1>405 Method Not Allowed</h1>");
    }
    else if (req.status == 411)
    {
        res.setStatus(411, "Length Required");
        res.setBody("<h1>411 Length Required</h1>");
    }
    else if (req.status == 413)
    {
        res.setStatus(413, "Payload Too Large");
        res.setBody("<h1>413 Payload Too Large</h1>");
    }
    else if (req.status == 414)
    {
        res.setStatus(414, "URI Too Long");
        res.setBody("<h1>414 URI Too Long</h1>");
    }
    else if (req.status == 501)
    {
        res.setStatus(501, "Not Implemented");
        res.setBody("<h1>501 Not Implemented</h1>");
    }
    else if (req.status == 508)
    {
        res.setStatus(508, "Loop Detected");
        res.setBody("<h1>508 Loop Detected</h1>");
    }
    else
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>500 Internal Server Error</h1>");
    }

    res.setHeader("Content-Type", "text/html");
    res.setHeader("Date", getHttpDate());

    std::map<int, std::string>::const_iterator it = conf.error_pages.find(req.status);
    if (it != conf.error_pages.end())
    {
        std::string content;
        if (readFile(it->second, content))
        {
            res.setBody(content);
            res.setHeader("Content-Type", getMimeType(it->second));
        }
    }

    return res;
}

Response ResponseHandler::handleGET(const std::string& path)
{
    Response res;
    std::string content;

    if (!readFile(path, content))
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>500 Internal Server Error</h1>");
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Date", getHttpDate());
        return res;
    }

    res.setStatus(200, "OK");
    res.setBody(content);
    res.setHeader("Content-Type", getMimeType(path));
    res.setHeader("Date", getHttpDate());

    return res;
}

// DELETE per RFC 2616 §9.7: a successful response SHOULD be 200 (OK) if
// the response includes an entity describing the status, 202 (Accepted)
// if the action has not yet been enacted, or 204 (No Content) if the
// action has been enacted but the response includes no entity. We return
// 200 with a short body describing the outcome, which is an explicitly
// permitted form.
Response ResponseHandler::handleDELETE()
{
    Response res;
    const std::string& path = req.confurm_path;
    struct stat st;

    res.setHeader("Date", getHttpDate());

    if (path.empty() || stat(path.c_str(), &st) != 0)
    {
        res.setStatus(404, "Not Found");
        res.setBody("<h1>404 Not Found</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    if (S_ISDIR(st.st_mode))
    {
        res.setStatus(403, "Forbidden");
        res.setBody("<h1>403 Forbidden (Directory)</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    if (access(path.c_str(), W_OK) != 0)
    {
        res.setStatus(403, "Forbidden");
        res.setBody("<h1>403 Forbidden (No permission)</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    if (std::remove(path.c_str()) != 0)
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>500 Could not delete file</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    res.setStatus(200, "OK");
    res.setBody("<h1>File Deleted Successfully</h1>");
    res.setHeader("Content-Type", "text/html");

    return res;
}

// POST/upload per RFC 1945 §9.6: a 201 Created response MUST include a
// Location header identifying the newly created resource's URI.
Response ResponseHandler::handlePOST()
{
    Response res;

    res.setHeader("Date", getHttpDate());

    if (conf.client_max_body_size > 0 && req.body.size() > conf.client_max_body_size)
    {
        res.setStatus(413, "Payload Too Large");
        res.setBody("<h1>413 Payload Too Large</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    const std::string& dir = req.confurm_path;
    if (dir.empty())
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>Upload target not configured</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    struct stat dir_st;
    if (stat(dir.c_str(), &dir_st) != 0 || !S_ISDIR(dir_st.st_mode))
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>Upload directory does not exist</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    std::string originalName = getOriginalFilename(req);
    std::string ext = resolveUploadExt(req, originalName);

    // Atomically claim "upload<N><ext>" so two concurrent uploads can
    // never win the same filename. O_CREAT|O_EXCL performs the
    // check-and-create as a single kernel operation, closing the race
    // that a separate stat()-then-open() would leave open.
    std::string fileName;
    int fd = -1;
    const int maxAttempts = 100000;
    for (int i = 1; i < maxAttempts; ++i)
    {
        fileName = joinPath(dir, "upload" + toString(i) + ext);
        fd = ::open(fileName.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
        if (fd >= 0)
            break;
        if (errno != EEXIST)
        {
            res.setStatus(500, "Internal Server Error");
            res.setBody("<h1>500 Internal Server Error</h1>");
            res.setHeader("Content-Type", "text/html");
            return res;
        }
    }

    if (fd < 0)
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>500 Could not allocate upload filename</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    size_t offset = 0;
    bool writeFailed = false;
    while (offset < req.body.size())
    {
        ssize_t n = ::write(fd, req.body.data() + offset, req.body.size() - offset);
        if (n <= 0)
        {
            writeFailed = true;
            break;
        }
        offset += static_cast<size_t>(n);
    }
    ::close(fd);

    if (writeFailed)
    {
        std::remove(fileName.c_str());
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>500 Internal Server Error</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    std::string basename = fileName.substr(fileName.find_last_of('/') + 1);
    logUploadMapping(dir, basename, originalName);

    // RFC 1945 §9.6: 201 Created MUST carry a Location header pointing
    // to the new resource.
    std::string location = req.path;
    if (!location.empty() && location[location.size() - 1] != '/')
        location += "/";
    location += basename;

    res.setStatus(201, "Created");
    res.setHeader("Location", location);
    res.setBody("<h1>Upload successful</h1><p>Created at " + location + "</p>");
    res.setHeader("Content-Type", "text/html");
    if (!originalName.empty())
        res.setHeader("X-Original-Filename", originalName);

    return res;
}

Response ResponseHandler::handleAutoIndex(const std::string& path)
{
    Response res;
    DIR* dir = opendir(path.c_str());

    res.setHeader("Date", getHttpDate());

    if (!dir)
    {
        res.setStatus(500, "Internal Server Error");
        res.setBody("<h1>500 Internal Server Error</h1>");
        res.setHeader("Content-Type", "text/html");
        return res;
    }

    std::string body;
    body += "<html><body>";
    body += "<h1>Index of " + path + "</h1>";
    body += "<ul>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;

        if (name == "." || name == "..")
            continue;
        // uploads.log is bookkeeping metadata, not a browsable resource.
        if (name == "uploads.log")
            continue;

        body += "<li><a href=\"" + name + "\">" + name + "</a></li>";
    }

    body += "</ul>";
    body += "</body></html>";

    closedir(dir);

    res.setStatus(200, "OK");
    res.setBody(body);
    res.setHeader("Content-Type", "text/html");

    return res;
}

Response ResponseHandler::handle()
{
    if (req.is_cgi)
        return handleCGI();

    if (req.status == 1001)
        return handleAutoIndex(req.confurm_path);

    if (req.status != 200)
        return handleReqErrors();

    if (req.method == "GET")
        return handleGET(req.confurm_path);
    else if (req.method == "DELETE")
        return handleDELETE();
    else if (req.method == "POST")
        return handlePOST();

    Response res;
    res.setStatus(405, "Method Not Allowed");
    res.setBody("<h1>405 Method Not Allowed</h1>");
    res.setHeader("Content-Type", "text/html");
    res.setHeader("Date", getHttpDate());
    if (res.getStatusCode() < 400)
    {
        std::map<std::string, std::string>::const_iterator it = req.query_params.find("user");
        std::string cookieValue;

        if (it != req.query_params.end() && !it->second.empty())
            cookieValue = it->second;
        else
            cookieValue = "visited_homepage";

        res.setCookie("webserv", cookieValue, "/", false);
    }

    return res;
}