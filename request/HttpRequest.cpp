#include "HttpRequest.hpp"
#include "../response/response.hpp"
#include <algorithm>

static std::string lowerCopy(const std::string& value)
{
    std::string result = value;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    return result;
}

static std::map<std::string, std::string>::const_iterator findHeaderInsensitive(
    const std::map<std::string, std::string>& headers,
    const std::string& wanted)
{
    const std::string wantedLower = lowerCopy(wanted);
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        if (lowerCopy(it->first) == wantedLower)
            return it;
    }
    return headers.end();
}

// ============================================================
// Free helper functions
// ============================================================

bool valid_request_line(const std::string& line)
{
    size_t firstSpace = line.find(' ');
    if (firstSpace == std::string::npos)
        return false;
    size_t secondSpace = line.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos)
        return false;
    if (line.find(' ', secondSpace + 1) != std::string::npos)
        return false;

    std::string method = line.substr(0, firstSpace);
    if (!(method == "GET" || method == "POST" || method == "DELETE"))
        throw std::logic_error("501"); // distinguishes "not implemented" from generic 400

    std::string version = line.substr(secondSpace + 1);
    if (version.substr(0, 5) != "HTTP/")
        return false;

    return true;
}

std::string parse_method(const std::string& str)
{
    size_t spacePos = str.find(' ');
    std::string firstWord = str.substr(0, spacePos);
    if (firstWord == "GET" || firstWord == "POST" || firstWord == "DELETE")
        return firstWord;
    throw std::logic_error("501");
}

std::string parse_version(const std::string& str)
{
    size_t lastSpace = str.find_last_of(' ');
    return str.substr(lastSpace + 1);
}

std::string parse_path(const std::string& str)
{
    size_t firstSpace = str.find(' ');
    size_t secondSpace = str.find(' ', firstSpace + 1);
    std::string secondWord = str.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    size_t qmark = secondWord.find('?');
    if (qmark != std::string::npos)
        return secondWord.substr(0, qmark);
    return secondWord;
}

std::string trim_str(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

std::map<std::string, std::string> pars_heders(const std::vector<std::string>& lines)
{
    std::map<std::string, std::string> ret;
    for (unsigned int i = 1; i < lines.size(); i++) {
        if (lines[i].find_first_not_of(" \t\n\r\f\v") == std::string::npos)
            break;
        size_t colon_pos = lines[i].find(':');
        if (colon_pos == std::string::npos || colon_pos == 0)
            throw std::invalid_argument("Malformed header line");
        std::string key = trim_str(lines[i].substr(0, colon_pos));
        std::string value = trim_str(lines[i].substr(colon_pos + 1));
        if (key.empty())
            throw std::invalid_argument("Empty header key");
        ret.insert(std::make_pair(key, value));
    }
    return ret;
}

std::string pars_body(const std::vector<std::string>& lines)
{
    unsigned int i;
    for (i = 1; i < lines.size(); i++) {
        if (lines[i].find_first_not_of(" \t\n\r\f\v") == std::string::npos)
            break;
    }
    i += 1;
    if (i == lines.size())
        return "";

    std::string ret;
    while (i < lines.size()) {
        ret += lines[i];
        if (i != lines.size() - 1)
            ret += "\n";
        i++;
    }
    return ret;
}

static std::map<std::string, std::string> parse_cookies(const std::map<std::string, std::string>& headers)
{
    std::map<std::string, std::string> cookies;
    std::map<std::string, std::string>::const_iterator it = headers.find("Cookie");

    if (it == headers.end())
        return cookies;
    return Cookie::parse(it->second);
}

std::vector<std::string> split(const std::string& str, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter))
        result.push_back(token);
    return result;
}

std::map<std::string, std::string> pars_query(const std::string& str)
{
    std::map<std::string, std::string> ret;
    size_t qmark = str.find('?');
    if (qmark == std::string::npos)
        return ret;

    size_t spacePos = str.find(' ', qmark);
    std::string theword = (spacePos == std::string::npos)
        ? str.substr(qmark + 1)
        : str.substr(qmark + 1, spacePos - qmark - 1);

    std::vector<std::string> pairs = split(theword, '&');
    for (unsigned int i = 0; i < pairs.size(); i++) {
        size_t eq_pos = pairs[i].find('=');
        std::string key, value;
        if (eq_pos == std::string::npos) {
            key = pairs[i];
            value = "";
        } else {
            key = pairs[i].substr(0, eq_pos);
            value = pairs[i].substr(eq_pos + 1);
        }
        ret.insert(std::make_pair(key, value));
    }
    return ret;
}

const ServerConfig::LocationConfig* best_match_location(const std::string& path, const ServerConfig& serv)
{
    const ServerConfig::LocationConfig* best = NULL;
    size_t bestLen = 0;
    for (size_t i = 0; i < serv.locations.size(); ++i) {
        const std::string& prefix = serv.locations[i].prefix;
        if (path.rfind(prefix, 0) == 0 && prefix.size() > bestLen) {
            bestLen = prefix.size();
            best = &serv.locations[i];
        }
    }
    return best;
}

std::string toLower(const std::string& str)
{
    std::string result = str;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = std::tolower(result[i]);
    return result;
}

bool method_allowed(const std::string& method, const ServerConfig::LocationConfig* loc)
{
    if (!loc) return true;
    if (loc->methods.empty()) return true;
    return loc->methods.count(toLower(method)) != 0;
}

// Strip a location's prefix from a path and guarantee a leading '/'.
// Only strips when the location defines a custom root and is not the "/" location,
// matching the original root-mapping semantics.
static std::string strip_location_prefix(const std::string& path,
                                          const ServerConfig::LocationConfig* loc)
{
    std::string path_to_use = path;

    if (loc && !loc->root.empty() && loc->prefix != "/") {
        if (path_to_use.find(loc->prefix) == 0)
            path_to_use = path_to_use.substr(loc->prefix.length());
    }

    if (!path_to_use.empty() && path_to_use[0] != '/')
        path_to_use = "/" + path_to_use;

    return path_to_use;
}

static std::string strip_trailing_slash(std::string s)
{
    if (!s.empty() && s[s.size() - 1] == '/')
        s = s.substr(0, s.size() - 1);
    return s;
}

// ============================================================
// Filesystem-level checks per method (GET / POST upload / DELETE)
// ============================================================

void check_path_get(validat& requ, const std::string& fs_path,
                     const ServerConfig::LocationConfig* loc, const std::string& method)
{
    // -------------------------
    // POST (upload-like behavior)
    // -------------------------
    if (method == "POST")
    {
        if (!loc || !loc->upload.enabled) {
            requ.code = 403;
            requ.path = "";
            return;
        }
        if (loc->upload.dir.empty()) {
            requ.code = 500;
            requ.path = "";
            return;
        }

        struct stat upload_st;
        if (stat(loc->upload.dir.c_str(), &upload_st) != 0) {
            if (errno == ENOENT || errno == ENOTDIR)
                requ.code = 404;
            else if (errno == EACCES || errno == EPERM)
                requ.code = 403;
            else
                requ.code = 500;
            requ.path = "";
            return;
        }

        if (!S_ISDIR(upload_st.st_mode)) {
            requ.code = 403;
            requ.path = "";
            return;
        }

        if (access(loc->upload.dir.c_str(), W_OK | X_OK) != 0) {
            requ.code = (errno == EACCES || errno == EPERM) ? 403 : 500;
            requ.path = "";
            return;
        }

        requ.code = 200;
        requ.path = loc->upload.dir; // directory where the upload handler should write
        return;
    }

    struct stat st;

    if (stat(fs_path.c_str(), &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR)
            requ.code = 404;
        else if (errno == ENAMETOOLONG)
            requ.code = 414;
        else if (errno == EACCES || errno == EPERM)
            requ.code = 403;
        else
            requ.code = 500;
        requ.path = "";
        return;
    }

    // -------------------------
    // DELETE
    // -------------------------
    if (method == "DELETE")
    {
        if (S_ISDIR(st.st_mode)) {
            requ.code = 403; // deleting directories not supported
            requ.path = "";
            return;
        }
        if (access(fs_path.c_str(), W_OK) != 0) {
            requ.code = 403;
            requ.path = "";
            return;
        }
        requ.code = 200;
        requ.path = fs_path;
        return;
    }

    // -------------------------
    // GET
    // -------------------------
    if (S_ISDIR(st.st_mode))
    {
        for (size_t i = 0; loc && i < loc->index.size(); i++) {
            std::string index_path = fs_path + loc->index[i];
            struct stat index_st;
            if (stat(index_path.c_str(), &index_st) == 0 &&
                S_ISREG(index_st.st_mode) &&
                access(index_path.c_str(), R_OK) == 0)
            {
                requ.code = 200;
                requ.path = index_path;
                return;
            }
        }
        if (loc && loc->autoindex) {
            requ.code = 1001; // directory listing
            requ.path = fs_path;
            return;
        }
        requ.code = 403;
        requ.path = "";
        return;
    }

    if (access(fs_path.c_str(), R_OK) != 0) {
        requ.code = (errno == EACCES || errno == EPERM) ? 403 : 500;
        requ.path = "";
        return;
    }

    requ.code = 200;
    requ.path = fs_path;
}

// ============================================================
// HttpRequest member functions
// ============================================================
validat HttpRequest::validate_request(const ServerConfig& serv)
{
    std::string current_path = this->path;
    const ServerConfig::LocationConfig* loc = best_match_location(current_path, serv);
    validat requ;

    // --- Redirect (single hop; caller/client follows further redirects) ---
    if (loc && loc->redirect.enabled)
    {
        requ.code = loc->redirect.code;
        this->redirect_target = loc->redirect.target;
        return requ;
    }

    if (!method_allowed(this->method, loc)) {
        requ.code = 405;
        requ.path = "";
        return requ;
    }

    // --- CGI-enabled location: map the request to the script file itself ---
    if (loc && !loc->cgi.empty())
    {
        std::string root = serv.root;
        if (loc && !loc->root.empty())
            root = loc->root;
        root = strip_trailing_slash(root);

        std::string path_to_use = strip_location_prefix(this->path, loc);
        std::string fs_path = root + path_to_use;

        std::string::size_type dot_pos = fs_path.find_last_of('.');
        if (dot_pos != std::string::npos)
        {
            std::string ext = fs_path.substr(dot_pos);
            if (loc->cgi.find(ext) != loc->cgi.end())
            {
                struct stat st;
                if (stat(fs_path.c_str(), &st) != 0)
                {
                    if (errno == ENOENT || errno == ENOTDIR)
                        requ.code = 404;
                    else if (errno == EACCES || errno == EPERM)
                        requ.code = 403;
                    else
                        requ.code = 500;
                    requ.path = "";
                    return requ;
                }
                if (!S_ISREG(st.st_mode))
                {
                    requ.code = 403;
                    requ.path = "";
                    return requ;
                }
                if (access(fs_path.c_str(), R_OK) != 0)
                {
                    requ.code = (errno == EACCES || errno == EPERM) ? 403 : 500;
                    requ.path = "";
                    return requ;
                }

                requ.code = 200;
                requ.path = fs_path;
                return requ;
            }
        }
    }

    // --- Upload-enabled location: write target is the upload dir, not document root ---
    if (loc && loc->upload.enabled && !loc->upload.dir.empty() &&
        this->path.find(loc->prefix) == 0)
    {
        struct stat st;
        if (stat(loc->upload.dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            requ.code = 500;
            requ.path = "";
            return requ;
        }

        std::string upload_dir = strip_trailing_slash(loc->upload.dir);
        std::string path_to_use = strip_location_prefix(this->path, loc);

        requ.code = 200;
        requ.path = upload_dir + path_to_use;
        return requ;
    }

    // --- Default: map request path onto document root ---
    std::string root = serv.root;
    if (loc && !loc->root.empty())
        root = loc->root;
    root = strip_trailing_slash(root);

    // Guard against a null loc: no location matched, so there's no
    // prefix to strip — use the raw request path against the server root.
    std::string path_to_use = loc ? strip_location_prefix(this->path, loc) : this->path;
    std::string fs_path = root + path_to_use;

    check_path_get(requ, fs_path, loc, this->method);
    return requ;
}

HttpRequest::HttpRequest(const std::string& raw_request, const ServerConfig& serv)
{
    const size_t DEFAULT_MAX_BODY = 1024 * 1024 * 5;
    const size_t MAX_BODY = serv.client_max_body_size ? serv.client_max_body_size : DEFAULT_MAX_BODY;
    const size_t MAX_PATH = 2048;

    this->status = 200;
    this->confurm_path = "";
    this->redirect_target = "";
    this->is_cgi = false;

    try {
        if (raw_request.empty())
            throw std::invalid_argument("Request is empty");

        std::vector<std::string> lines;
        std::stringstream ss(raw_request);
        std::string line;
        while (std::getline(ss, line))
            lines.push_back(line);

        if (lines.empty())
            throw std::invalid_argument("Request is not valid: no lines present");

        if (!valid_request_line(lines[0]))
            throw std::invalid_argument("Malformed request line");

        this->method  = parse_method(lines[0]);
        this->query_string = pars_query(lines[0]).empty() ? "" : lines[0].substr(lines[0].find(' ') + 1, lines[0].find(' ', lines[0].find(' ') + 1) - lines[0].find(' ') - 1);
        size_t query_pos = this->query_string.find('?');
        if (query_pos != std::string::npos)
            this->query_string = this->query_string.substr(query_pos + 1);
        this->path    = parse_path(lines[0]);
        this->version = parse_version(lines[0]);

        if (this->path.size() > MAX_PATH) {
            this->status = 414;
            return;
        }

        this->headers = pars_heders(lines);
        this->cookies = parse_cookies(this->headers);

        if ((this->version == "HTTP/1.1" || this->version == "HTTP/2.0") &&
            findHeaderInsensitive(this->headers, "Host") == this->headers.end()) {
            this->status = 400;
            return;
        }

        if (this->method == "POST" && findHeaderInsensitive(this->headers, "Content-Length") == this->headers.end()) {
            this->status = 411;
            return;
        }

        this->body = pars_body(lines);

        if (this->method == "POST") {
            std::map<std::string, std::string>::const_iterator it = findHeaderInsensitive(this->headers, "Content-Length");
            if (it != this->headers.end()) {
                std::istringstream iss(it->second);
                size_t body_len = 0;
                iss >> body_len;
                if (!iss.fail() && body_len > MAX_BODY) {
                    this->status = 413;
                    return;
                }
            }
            if (this->body.size() > MAX_BODY) {
                this->status = 413;
                return;
            }
        }

        this->query_params = pars_query(lines[0]);

        validat result = validate_request(serv); // single call, no duplicate work
        this->status = result.code;
        this->confurm_path = result.path;

        // --- CGI detection / environment setup ---
        if (this->status == 200 || this->status == 1001) {
            this->detect_cgi_request(serv);

            if (this->is_cgi && (this->status == 200 || this->status == 1001))
                this->setup_cgi_environment(serv, serv.listens.empty() ? 0 : serv.listens[0].port);
        }
    }
    catch (const std::logic_error& e) {
        this->status = (std::string(e.what()) == "501") ? 501 : 400;
        this->confurm_path = "";
        this->is_cgi = false;
    }
    catch (const std::exception& e) {
        (void)e;
        this->status = 400;
        this->confurm_path = "";
        this->is_cgi = false;
    }
}

bool HttpRequest::detect_cgi_request(const ServerConfig& serv)
{
    const ServerConfig::LocationConfig* best = best_match_location(this->path, serv);

    if (!best || best->cgi.empty()) {
        is_cgi = false;
        return false;
    }

    size_t dot_pos = confurm_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        is_cgi = false;
        return false;
    }

    cgi_extension = confurm_path.substr(dot_pos);

    std::map<std::string, std::string>::const_iterator cgi_it = best->cgi.find(cgi_extension);
    if (cgi_it == best->cgi.end()) {
        status = 403;
        is_cgi = false;
        return false;
    }

    cgi_interpreter = cgi_it->second;
    cgi_script_path = confurm_path;
    is_cgi = true;
    return true;
}

void HttpRequest::setup_cgi_environment(const ServerConfig& serv, int serverPort)
{
    if (!is_cgi) return;
    cgi_env.clear();

    cgi_env["REQUEST_METHOD"] = method;
    cgi_env["QUERY_STRING"]   = query_string;
    cgi_env["CONTENT_LENGTH"] = intToString(body.length());

    std::map<std::string, std::string>::const_iterator it_content = findHeaderInsensitive(headers, "Content-Type");
    cgi_env["CONTENT_TYPE"] = (it_content != headers.end()) ? it_content->second : "";

    cgi_env["SCRIPT_NAME"]     = path;
    cgi_env["SCRIPT_FILENAME"] = cgi_script_path;
    cgi_env["PATH_INFO"]       = "";

    cgi_env["SERVER_NAME"]     = serv.server_name;
    if (serverPort > 0)
        cgi_env["SERVER_PORT"] = intToString(serverPort);
    else if (!serv.listens.empty())
        cgi_env["SERVER_PORT"] = intToString(serv.listens[0].port);
    else
        cgi_env["SERVER_PORT"] = "";
    cgi_env["SERVER_PROTOCOL"] = "HTTP/1.1";
    cgi_env["GATEWAY_INTERFACE"] = "CGI/1.1";

    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        std::string key = it->first;
        if (key == "Content-Type" || key == "Content-Length")
            continue;

        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        for (std::string::iterator c = key.begin(); c != key.end(); ++c) {
            if (*c == '-') *c = '_';
        }
        key = "HTTP_" + key;
        cgi_env[key] = it->second;
    }
}

void HttpRequest::reqq()
{
    std::cout
        << "method: "          << this->method          << "\n"
        << "path: "            << this->path            << "\n"
        << "confurm_path: "    << this->confurm_path    << "\n"
        << "version: "         << this->version          << "\n"
        << "body: "            << this->body            << "\n"
        << "status: "          << this->status           << "\n"
        << "redirect_target: " << this->redirect_target << "\n";

    for (std::map<std::string, std::string>::iterator it = this->headers.begin();
         it != this->headers.end(); ++it)
        std::cerr << "header: " << it->first << " = " << it->second << "\n";

    for (std::map<std::string, std::string>::iterator it = this->query_params.begin();
         it != this->query_params.end(); ++it)
        std::cerr << "query: " << it->first << " = " << it->second << "\n";

    std::cout << "\n--- CGI Information ---\n";
    std::cout << "is_cgi: " << (this->is_cgi ? "true" : "false") << "\n";

    if (this->is_cgi) {
        std::cout << "cgi_script_path: " << this->cgi_script_path << "\n"
                  << "cgi_extension: "   << this->cgi_extension   << "\n"
                  << "cgi_interpreter: " << this->cgi_interpreter << "\n"
                  << "query_string: "    << this->query_string    << "\n";

        std::cout << "\nCGI Environment Variables:\n";
        for (std::map<std::string, std::string>::iterator it = this->cgi_env.begin();
             it != this->cgi_env.end(); ++it)
            std::cerr << "  " << it->first << " = " << it->second << "\n";
    }
}
