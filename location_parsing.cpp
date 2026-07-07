#include "configloader.hpp"
#include "configtypes.hpp"

#include <cctype>
#include <cstdlib>
#include <string>

void parseLocationBlock(TokenList::const_iterator& it,TokenList::const_iterator end,ServerConfig& server,int serverWordLine)
{
    expectWord(it, end, "location");

    if (it == end || it->type != TOK_WORD)
        throw ParseError(
            "Parse error: location expects a prefix",
            (it == end ? -1 : it->line)
        );

    ServerConfig::LocationConfig loc;
    LocationInfo locDir;

    loc.prefix = it->text;
    locDir.content["prefix"].seen = true;
    expect(it, end, TOK_WORD);

    const int locOpenBraceLine = (it != end ? it->line : -1);
    expect(it, end, TOK_LBRACE);

    while (it != end && it->type != TOK_RBRACE && it->type != TOK_EOF)
    {
        if (it->type != TOK_WORD)
            throw ParseError("Parse error: expected a directive inside location block", it->line);

        const std::string directive = to_lower(it->text);

        if (locDir.content.find(directive) != locDir.content.end())
        {
            if (!locDir.content[directive].allowMultiple && locDir.content[directive].seen)
                throw ParseError(
                    "Parse error: duplicate directive '" + directive + "' in location block",
                    it->line
                );
            locDir.content[directive].seen = true;
        }

        if (directive == "allowed_methods")
        {
            expectWord(it, end, "allowed_methods");

            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: allowed_methods expects at least one method",
                    (it == end ? -1 : it->line)
                );

            while (it != end && it->type == TOK_WORD)
            {
                std::string method = to_lower(it->text);
                if (method != "get" && method != "post" && method != "delete")
                    throw ParseError("Parse error: invalid HTTP method '" + it->text + "'", it->line);
                loc.methods.insert(method);
                expect(it, end, TOK_WORD);
            }
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "return" || directive == "redirect")
        {
            // Accept both 'return' (as used by our sample config) and 'redirect' (legacy naming)
            expect(it, end, TOK_WORD);

            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: return expects a status code and a URL",
                    (it == end ? -1 : it->line)
                );

            int code = static_cast<int>(std::strtol(it->text.c_str(), NULL, 10));
            if (code <= 0)
                throw ParseError("Parse error: invalid return code '" + it->text + "'", it->line);
            expect(it, end, TOK_WORD);

            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: return expects a URL after the status code",
                    (it == end ? -1 : it->line)
                );

            loc.redirect.enabled = true;
            loc.redirect.code = code;
            loc.redirect.target = it->text;
            expect(it, end, TOK_WORD);
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "root")
        {
            expectWord(it, end, "root");
            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: root expects a path inside location block",
                    (it == end ? -1 : it->line)
                );
            loc.root = it->text;
            expect(it, end, TOK_WORD);
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "autoindex")
        {
            expectWord(it, end, "autoindex");
            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: autoindex expects 'on' or 'off'",
                    (it == end ? -1 : it->line)
                );
            const std::string v = to_lower(it->text);
            if (v != "on" && v != "off")
                throw ParseError("Parse error: autoindex expects 'on' or 'off'", it->line);
            loc.autoindex = (v == "on");
            expect(it, end, TOK_WORD);
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "index")
        {
            expectWord(it, end, "index");
            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: index expects at least one filename",
                    (it == end ? -1 : it->line)
                );
            while (it != end && it->type == TOK_WORD)
            {
                loc.index.push_back(it->text);
                expect(it, end, TOK_WORD);
            }
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "upload")
        {
            expectWord(it, end, "upload");
            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: upload expects 'on' or 'off'",
                    (it == end ? -1 : it->line)
                );
            const std::string v = to_lower(it->text);
            if (v != "on" && v != "off")
                throw ParseError("Parse error: upload expects 'on' or 'off'", it->line);
            loc.upload.enabled = (v == "on");
            expect(it, end, TOK_WORD);
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "upload_dir")
        {
            expectWord(it, end, "upload_dir");
            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: upload_dir expects a path",
                    (it == end ? -1 : it->line)
                );
            loc.upload.enabled = true;
            loc.upload.dir = it->text;
            expect(it, end, TOK_WORD);
            expect(it, end, TOK_SEMI);
        }
        else if (directive == "cgi" || directive == "cgi_ext")
        {
            // Accept both 'cgi' (doc) and 'cgi_ext' (sample config)
            expect(it, end, TOK_WORD);

            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: cgi_ext expects an extension and a program path",
                    (it == end ? -1 : it->line)
                );
            const std::string ext = it->text;
            expect(it, end, TOK_WORD);

            if (it == end || it->type != TOK_WORD)
                throw ParseError(
                    "Parse error: cgi_ext expects a program path after the extension",
                    (it == end ? -1 : it->line)
                );
            const std::string prog = it->text;
            expect(it, end, TOK_WORD);

            loc.cgi[ext] = prog;
            expect(it, end, TOK_SEMI);
        }
        else
        {
            throw ParseError("Parse error: unknown directive '" + it->text + "' in location block", it->line);
        }
    }

    if (it == end || it->type == TOK_EOF)
    {
        throw ParseError(
            "Parse error: unexpected end of file; missing '}' to close location block opened here",
            (locOpenBraceLine > 0 ? locOpenBraceLine : serverWordLine)
        );
    }

    expect(it, end, TOK_RBRACE);

    server.locations.push_back(loc);
}
