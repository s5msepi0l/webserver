#ifndef COMMON_GUARD
#define COMMON_GUARD

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cassert>
#include <errno.h>
#include <unordered_map>
#include <string_view>

#include <sys/poll.h>

//self explanatory, mainly used inside asserts in network_controller
//constructor
#define uint_16_limit 0xFFFF
#define uint_32_limit 0xFFFFFFFF


//http request methods
// anything below post can eat shit
typedef enum {
  GET,
  POST,
  HEAD,
  PUT,
  DELETE,
  CONNECT,
  OPTIONS,
  TRACE,
  PATCH,
  EMPTY // client request corrupted or smething
}http_methods;

//unparsed data to be transferred from main thread to thread pool
//to be parsed and handled accordingly to reduce strain on main thread
typedef struct {
  unsigned long id;
  unsigned long fd;
  std::string data;
}request_packet;

typedef struct destination{
    http_methods method;
    std::string path;

    // comparison between struct, mainly used by std::map
    // might cause some problems by not checking string 
    // but that would slow it down significantly
    bool operator<(const struct destination& other) const {
        if (method < other.method) return true;
        if (method > other.method) return false;
        return path < other.path;
    }
} destination;

//parsed client data to be directly passed to client on a silver platter
//mostly works as a temporary buffer for the
typedef struct {
  int fd; 
  destination dest; 
  std::unordered_map<std::string, std::string> cookies;
} parsed_request;

typedef struct {
  int status;
  std::string data;
  std::string content_type;
  size_t length;
}packet_response;

std::string n_fetch(std::string path) { 
  std::ifstream fd(path);
  
  std::string buffer, line;
  if (fd.is_open()) {

    while (getline(fd, line)) {
      buffer.append(line);
    }

    fd.close();
  }

  return buffer;
}

std::vector<std::string> l_fetch(std::string path) { // used mainly for whitelisting
  std::ifstream fd(path);
  assert(fd.is_open());

  std::string line;
  std::vector<std::string> buffer;
  while (getline(fd, line)) {
    buffer.push_back(line);
  }  

  return buffer; 
}

parsed_request parse_http(const request_packet &request) {
    parsed_request httpRequest;
    std::string buffer(request.data);

    // find method
    int methodEnd = buffer.find(' ');
    if (methodEnd != std::string::npos) {
        std::string tmp = buffer.substr(0, methodEnd);

        // can't really make switch case with strings as that would requre an entire function call
        if (tmp == "GET")  httpRequest.dest.method= GET;
        else if (tmp == "POST") httpRequest.dest.method= POST;
        else if (tmp == "HEAD") httpRequest.dest.method= HEAD;
        else if (tmp == "PUT")  httpRequest.dest.method= PUT;
        else if (tmp == "DELETE")  httpRequest.dest.method= DELETE;
        else if (tmp == "CONNECT")  httpRequest.dest.method= CONNECT;
        else if (tmp == "OPTIONS")  httpRequest.dest.method = OPTIONS;
        else if (tmp == "TRACE")  httpRequest.dest.method = TRACE;
        else if (tmp == "PATCH")  httpRequest.dest.method = PATCH;
        else httpRequest.dest.method = EMPTY;
    }

    // find path
    int pathStart = methodEnd + 1;
    int pathEnd = buffer.find(' ', pathStart);
    if (pathEnd != std::string::npos)
        httpRequest.dest.path = buffer.substr(pathStart, pathEnd - pathStart);

    // find cookies (if any)
    int cookieStart = buffer.find("Cookie: ");
    if (cookieStart != std::string::npos) {
        cookieStart += 8; // Skip "Cookie: "
        int cookieEnd = buffer.find("\r\n", cookieStart);
        std::string cookies = buffer.substr(cookieStart, cookieEnd - cookieStart);

        int separatorPos = cookies.find(';');
        int keyValueSeparatorPos;
        while (separatorPos != std::string::npos) {
            std::string cookie = cookies.substr(0, separatorPos);
            keyValueSeparatorPos = cookie.find('=');
            if (keyValueSeparatorPos != std::string::npos) {
                std::string key = cookie.substr(0, keyValueSeparatorPos);
                std::string value = cookie.substr(keyValueSeparatorPos + 1);
                httpRequest.cookies[key] = value;
            }

            cookies.erase(0, separatorPos + 1);
            separatorPos = cookies.find(';');
        }

        keyValueSeparatorPos = cookies.find('=');
        if (keyValueSeparatorPos != std::string::npos) {
            std::string key = cookies.substr(0, keyValueSeparatorPos);
            std::string value = cookies.substr(keyValueSeparatorPos + 1);
            httpRequest.cookies[key] = value;
        }
    }

    return httpRequest;
}

#endif