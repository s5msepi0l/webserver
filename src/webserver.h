/*
    RANT START:
        I genuenly don't understand a event driven server model,
        every time i try to implement it in a webserver or networking
        application the client just freezes for some reason not being able
        to receive the packet until the server application is closed
        even though i supposedly applied the nonblocking settings on the 
        listener socket and client fd. At this point I'm just lost
        as to why. I have used a threading model in the past utilising
        oversubscription for client handling on the larger scale but
        I doubt that would be a viable option in a generaly non cpu
        intensive session with a client mostly just fetching stuff from the cache.
        I'm not really sure if a event driven model would either be good solution
        when thread pools get involved as avoiding race conditions and mutex's
        would probably still be faster than a single threaded application
        but does apply significant limitations when a large amount of connection(s)
        are involved
    RANT END

Note: Http Keep-Alive support is done via threadpool with added load queuing
Note: Refrain from tweaking the networking structs too much as it may cause tech-debt

TODO: 
	- Add support http Keep-Alive support via multithreading
	- Implement Keep-Alive server-size timeout
	- Add optional ip-address whitelisting
	- Add optional Data logging
	- Implement automatic chunking based on cache size when retrieved
	- Make JSON implementation less "fucky" to work with
	- Add basic json support for receiving packet and sending
*/


#pragma once

#include "threadpool.h"
#include "cache.h"

#include <iostream>
#include <queue>
#include <atomic>

#include <map>
#include <unordered_set> // bst

#include <functional>
#include <map>
#include <queue>
#include <cstring>

#ifdef _WIN32

#include <winSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#endif

#ifdef __linux__
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
	#include <cerrno> // idk if windows supports perror too but im too lazy to crosscheck

    #define closesocket(fd) close(fd)


    #define SOCKET int

    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
#endif

#define _min(a, b) ((a) < (b) ? (a) : (b))

#define BUFFER_SIZE 8192
#define CACHE_SIZE 5

inline int _send(SOCKET fd, std::string buffer) {
    return send(fd, buffer.c_str(), buffer.size(), 0);
}

inline int _recv(SOCKET fd, std::string &buffer, bool appval) {
    if (!appval) buffer.clear();

	char data_buffer[BUFFER_SIZE]{ 0 };
    size_t bytes_recv;
	if ((bytes_recv = recv(fd, data_buffer, BUFFER_SIZE - 1, 0)) < 0)
        return -1;

	data_buffer[bytes_recv] = '\0'; // as to not fill the entire buffer with the buffer amount
	buffer += data_buffer; //assuming that the buffer param will be empty when passed
    
    return bytes_recv;
}

typedef enum {
    GET,
    POST
}methods;

//unparsed data to be transferred from main thread to thread pool
//to be parsed and handled accordingly to reduce strain on main thread
typedef struct {
    SOCKET fd;
    std::string data;
}request_packet;

typedef struct destination {
    methods method;
    std::string path;

    // comparison between struct, mainly used by std::map
    // might cause some problems by not checking string 
    // but that would slow it down significantly
    bool operator==(const struct destination& other) const {
        return method == other.method && other.path == path;
    }
} destination;

namespace std {
    template <>
    struct hash<destination> {
        size_t operator()(const destination& dest) const {
            // Combine the hash values of 'method' and 'path'
            size_t hash = 0;
            hash = hash_combine(hash, dest.method);
            hash = hash_combine(hash, std::hash<std::string>{}(dest.path));
            return hash;
        }

        // Only god and i knew what i was doing while i wrote this, now only god knows
        template <typename T>
        size_t hash_combine(size_t seed, const T& val) const {
            return seed ^ (std::hash<T>{}(val)+0x9e3779b9 + (seed << 6) + (seed >> 2));
        }
    };
}

//parsed packet data to be directly passed to client on a silver platter
typedef struct {
    SOCKET fd;
    destination dest;
    std::unordered_map<std::string, std::string> cookies;
} parsed_request;

typedef struct {
    int status;
    std::string body;
    std::string content_type;
    size_t length;

    int chunked;

    void _send_all(std::string cache_mem, parsed_request &request) {
        long cache_size = cache_mem.size();
        std::stringstream res;
        res << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: " << this->content_type << "\r\n"
            << "Transfer-Encoding: chunked\r\n"
            << "\r\n";

        // Send the HTTP headers
        const std::string &responseStr = res.str();
        send(request.fd, responseStr.c_str(), responseStr.length(), 0);

        long bytes_sent = 0;
        while (bytes_sent < cache_size)
        {
            std::cout << "sending chunked packet\n";
            long chunk_size = _min(BUFFER_SIZE, cache_size - bytes_sent);

            std::string chunk_header;
            chunk_header.reserve((chunk_size % 15) + 2);
            chunk_header.append(hexify(chunk_size));
            chunk_header.append("\r\n");
            send(request.fd, chunk_header.c_str(), chunk_header.size(), 0);

            send(request.fd, cache_mem.c_str() + bytes_sent, chunk_size, 0);

            send(request.fd, "\r\n", 2, 0);

            bytes_sent += chunk_size;
        }

        // final packet indicating EOF
        send(request.fd, "0\r\n\r\n", 5, 0);
    }

    inline void _send(std::string cache_mem, parsed_request &request) { // single packet response
        send(request.fd, cache_mem.c_str(), cache_mem.size(), 0);
    }
}packet_response;

inline void set_body_content(std::string path, packet_response &content) {
    content.body = path;
    content.chunked = 0;
}

inline void set_body_media_content(std::string path, packet_response &content) {
    content.body = path;
    content.chunked = 1;
}

inline void set_content_type(std::string type, packet_response &content) {
    content.content_type = type;
}

inline void set_content_status(int code, packet_response& content) {
    content.status = code;
}

inline void display_packet(packet_response Src) {
    std::cout << "status: " << Src.status << '\n'
        << "length: " << Src.length << '\n'
        << "content-type: " << Src.content_type << '\n'
        << "body: " << Src.body << std::endl;
}

namespace http {
    class packet_parser {
    public: //variables
        std::unordered_map<int, std::string> status_codes;

    public: //public interface logic
        packet_parser() {
            status_codes[200] = "OK";
            status_codes[501] = "Not Implemented";
         }

        std::string format(packet_response Src) {
            std::string buffer;
            buffer += "HTTP/1.1 " + std::string(std::to_string(Src.status) + " " + status_codes[Src.status] + "\r\n");
            buffer += "Content-Type: " + Src.content_type+ "\r\n";
            buffer += ("Content-Length: " + std::to_string(Src.length) + "\r\n\r\n");
            buffer += Src.body;

            buffer.append("\r\n\r\n");

            return buffer;
        }

		//wrote this ages ago no idea how or why it works
        parsed_request parse(const request_packet& request) {
            parsed_request httpRequest;
            std::string buffer(request.data);

            // find method
            int methodEnd = buffer.find(' ');
            if (methodEnd != std::string::npos) {
                std::string tmp = buffer.substr(0, methodEnd);

                // can't really make switch case with strings as that would requre an entire function call
                if (tmp == "GET")  httpRequest.dest.method = GET;
                else if (tmp == "POST") httpRequest.dest.method = POST;
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

            httpRequest.fd = request.fd;
            return httpRequest;
        }
    };

	class request_router {
	private:
        file_cache cache;
        packet_parser parser;
        std::unordered_map<destination, std::function<void(parsed_request&, packet_response&)>> routes;

    public:
        request_router(std::string cache_path) :
            cache(cache_path, CACHE_SIZE) {
        }

        inline void insert(destination path, std::function<void(parsed_request&, packet_response&)> route) {
            routes[path] = route;
        }

		// executed by threadpool contains 90% of all important routing and miscellaneous functionality
        void execute(request_packet client) {
            std::cout << "client thread start\n";
			int packets_sent = 0;	
			while (1) {	
			int status = _recv(client.fd, client.data, false);
			std::cout << "connection status: " << status << std::endl;
			if (status <= 0) break;

            parsed_request request = parser.parse(client);
            packet_response response;

			// ugly ass giant if-else statement, change this asap possibly to a swtich case via enum's
            std::function<void(parsed_request&, packet_response&)> func = routes[request.dest];
			if (func == nullptr) { 
                std::cout << "no route found\n" << request.dest.path << " path \n" << request.dest.method << std::endl;

                std::string buffer = "Unable to find requested path";
                response.status = 501;
                response.body = buffer;
                set_content_type("text/html", response);
            }
            else {
				std::cout << "excv interface\n";
                func(request, response);
                std::string cache_mem = cache.fetch(response.body);
                long cache_size = cache_mem.size();

                // file transfer
                if (response.chunked) {
                    response._send_all(cache_mem, request);
                }
                else {
					std::cout << "normal response\n\n\n\n\n\n\n\n";

					response.body = cache_mem;
                    response.length = cache_mem.size();
                    std::string packet_buffer = parser.format(response);
					

					//_send(request.fd, packet_buffer);
					std::cout << "sending response: " << send(request.fd, packet_buffer.c_str(), packet_buffer.size(), 0) << std::endl;
					packets_sent++;
                }
            }
				std::cout << "iterate\n";
			}


			std::cout << "closing: " << client.fd << std::endl;
			std::cout << "packet sent before closing: " << packets_sent << std::endl;
            
			if (closesocket(client.fd) == SOCKET_ERROR) {
				#ifdef _WIN32
                    std::cout << WSAGetLastError() << std::endl;
                #endif

                #ifdef __linux__
					throw "Unable to terminate tcp connection";
                #endif
					
            }
			
			
        }
    };

	class webserver {
	private:
        std::queue<request_packet> clients;
        request_router routes;
        nofetch_threadpool pool;

        SOCKET socket_fd;
        sockaddr_in server_addr;
        socklen_t server_len;

        std::thread worker;
        std::atomic<bool> flag;

    public:
        webserver(const char *cache_path, int port, int backlog, int thread_count):
        pool(thread_count),
        routes(std::string(cache_path)){
            socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (socket_fd == SOCKET_ERROR)
                throw std::runtime_error("Unable to initialize webserver socket");

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_len = sizeof(server_addr);

            if (bind(socket_fd, (sockaddr*)&server_addr, server_len) == SOCKET_ERROR)
                throw std::runtime_error("Unable to bind webserver address\n");


            if (listen(socket_fd, backlog) == SOCKET_ERROR)
                throw std::runtime_error("Unable to listen on port: " + std::to_string(port));
        
            flag.store(true);
            worker = std::thread(&http::webserver::acpt_worker, this);
        }

        void shutdown() {
            flag.store(false);
            pool.shutdown();
            worker.join();
        }

        inline void get(std::string path, std::function<void(parsed_request&, packet_response&)> route) {
            destination buf;
            buf.method = GET;
            buf.path = path;

            routes.insert(buf, route);
        }

        inline void post(std::string path, std::function<void(parsed_request&, packet_response&)> route) {
            destination buf;
            buf.method = POST;
            buf.path = path;

            routes.insert(buf, route);
        }

    private:
        void acpt_worker(void) {
            while (flag.load()) {
                SOCKET socket_buf = accept(socket_fd, (sockaddr*)&server_addr, &server_len);
                if (socket_buf == INVALID_SOCKET) {
                    #ifdef _WIN32
                        throw std::runtime_error("Unable to accept webclient connections\n");
                    #endif

                    #ifdef __linux__
						throw "Unable to accept webclient connections\n";
                    #endif
                }
                std::cout << "connection received\n";
                
                request_packet packet;
                packet.fd = socket_buf;
                packet.data = "";

                pool.exec([this, packet]() {
                    routes.execute(packet);
                 });
            }
        }
	};
};
