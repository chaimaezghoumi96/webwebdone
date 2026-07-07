CXX := g++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98

SRCS := \
	main.cpp \
	configloader.cpp \
	listen_parser.cpp \
	request/HttpRequest.cpp \
	WebServer.cpp \
	socket.cpp \
	location_parsing.cpp \
	response/response.cpp response/ResponseHandler.cpp \
	cookies/Cookie.cpp


OBJS := $(SRCS:.cpp=.o)
TARGET := webserv


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: re
	./$(TARGET) configurations/webserv.conf

re: clean all

clean:
	rm -f *.o $(OBJS)

fclean: clean
	rm -rf $(TARGET)

.PHONY: all fclean clean re run test
