#!/usr/bin/env python3
import os
import sys


def read_stdin():
	length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
	if length <= 0:
		return ""
	return sys.stdin.read(length)


def main():
	body = read_stdin()
	lines = [
		"Content-Type: text/plain; charset=utf-8",
		"",
		"CGI OK",
		"REQUEST_METHOD=" + os.environ.get("REQUEST_METHOD", ""),
		"QUERY_STRING=" + os.environ.get("QUERY_STRING", ""),
		"CONTENT_TYPE=" + os.environ.get("CONTENT_TYPE", ""),
		"CONTENT_LENGTH=" + os.environ.get("CONTENT_LENGTH", ""),
		"SCRIPT_NAME=" + os.environ.get("SCRIPT_NAME", ""),
		"SCRIPT_FILENAME=" + os.environ.get("SCRIPT_FILENAME", ""),
		"PATH_INFO=" + os.environ.get("PATH_INFO", ""),
		"BODY=" + body,
	]
	sys.stdout.write("\r\n".join(lines))


if __name__ == "__main__":
	main()
