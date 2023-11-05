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

Note: to change (any) of the finner details via macro defenitions you will have to
	define them before you include the header file, macro defenitions are documented in the readme
Note: Http Keep-Alive support is done via threadpool with added load queuing
Note: Refrain from tweaking the networking structs too much as it may cause tech-debt
Note: logs only contain connections information along with the amount of requests sent by the client

TODO:
	- Improve cache thread-safety & make webapp use common cache

	- Add optional ip-address whitelisting
	- Implement automatic chunking based on cache size when retrieved
	- Make JSON implementation less "fucky" to work with
	- Add basic json support for receiving packet and sending
	- Update logging for more customizability and robust functionality
*/


#pragma once

#include "threadpool.h"
#include "cache.h"
#include "logging.h"

#include <iostream>
#include <queue>
#include <atomic>

#include <map>
#include <unordered_set> // bst

#include <functional>
#include <map>
#include <queue>
#include <cstring>

#ifndef BUFFER_SIZE 
#define BUFFER_SIZE 8192
#endif

#ifndef CACHE_SIZE 
#define CACHE_SIZE 5
#endif

#ifndef CONNECTION_TIMEO
#define CONNECTION_TIMEO 300 // 5 minutes
#endif

#define CLIENT_CON "[Connection established]"
#define CLIENT_DISCON "[Connection terminated]"

#ifdef _WIN32

#include <winSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#endif

#ifdef __linux__
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
	#include <cerrno>	

	#include <sys/ioctl.h> // ip fetching
	#include <linux/if.h>

	#include <unistd.h>
	#include <cerrno> // idk if windows supports perror too but im too lazy to crosscheck

    #define closesocket(fd) close(fd)

    #define SOCKET int

    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
#endif

#define _min(a, b) ((a) < (b) ? (a) : (b))

inline int _send(SOCKET fd, std::string buffer) {
    return send(fd, buffer.c_str(), buffer.size(), 0);
}

inline int _recv(SOCKET fd, std::string &buffer, bool appval) {
    if (!appval) buffer.clear();

	char data_buffer[BUFFER_SIZE]{ 0 };
    size_t bytes_recv;
	bytes_recv = recv(fd, data_buffer, BUFFER_SIZE - 1, 0);

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
	bool keep_alive;
    destination dest;
    std::unordered_map<std::string, std::string> cookies;
} parsed_request;

typedef struct packet_response {
	SOCKET fd;

    int status;
    std::string body;
    std::string content_type;
    size_t length;

	packet_response(SOCKET Dst): fd(Dst) {}

	int _send(std::string Src) {
		if (Src.size() < BUFFER_SIZE)
			return send(fd, Src.c_str(), Src.size(), 0);
		
		return _send_all(Src);
	}

    inline int _send_all(std::string cache_mem) {
        long cache_size = cache_mem.size();
        std::stringstream res;
        res << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: " << this->content_type << "\r\n"
            << "Transfer-Encoding: chunked\r\n"
            << "\r\n";

        // Send the HTTP headers
        const std::string &response_str = res.str();
        send(this->fd, response_str.c_str(), response_str.length(), 0);

        long bytes_sent = 0;
        while (bytes_sent < cache_size)
        {
            std::cout << "sending chunked packet\n";
            long chunk_size = _min(BUFFER_SIZE, cache_size - bytes_sent);

            std::string chunk_header;
            chunk_header.reserve((chunk_size % 15) + 2);
            chunk_header.append(hexify(chunk_size));
            chunk_header.append("\r\n");
            send(this->fd, chunk_header.c_str(), chunk_header.size(), 0);

            send(this->fd, cache_mem.c_str() + bytes_sent, chunk_size, 0);

            send(this->fd, "\r\n", 2, 0);

            bytes_sent += chunk_size;
        }

        // final packet indicating EOF
        return send(this->fd, "0\r\n\r\n", 5, 0);
    }
	
	inline void set_body_content(std::string path) {
		this->body = path;
	}

	inline void set_content_type(std::string type) {
		this->content_type = type;
	}

	inline void set_content_status(int code) {
		this->status = code;
	}

	// mainly used for debugging purposes
	inline void display_packet(packet_response Src) {
		std::cout << "status: " << Src.status << '\n'
			<< "length: " << Src.length << '\n'
			<< "content-type: " << Src.content_type << '\n'
		    << "body: " << Src.body << std::endl;
	}
}packet_response;

// contains logging utility relevant to the webserver
namespace log_util {
	typedef struct log_msg {
		std::string level;
		std::string msg;
	} log_msg;


	log_msg OUT_OF_MEMORY { "WARNING", "Out of memory error"};
	log_msg CONNECTION_RECV { "[INFO]", "[Connection received]" };
}

namespace http {
	// Todo: finish this mess
	inline std::string fetch_ip(SOCKET fd) {
		struct sockaddr_in addr;
		socklen_t addr_size = sizeof(struct sockaddr_in);
		int res = getpeername(fd, (struct sockaddr *)&addr, &addr_size);
		
		char clientip[20];
		return std::string(inet_ntoa(addr.sin_addr));
	}

    std::string format_http(packet_response Src) {
		std::string status_code;
		switch(Src.status) {
			case 200:
				status_code = "OK";
				break;
					
			case 501:
				status_code = "Not Implemented";
				break;
		}

		std::string buffer;
        buffer += "HTTP/1.1 " + (std::to_string(Src.status) + " " + status_code + "\r\n");
        buffer += "Content-Type: " + Src.content_type+ "\r\n";
        buffer += ("Content-Length: " + std::to_string(Src.length) + "\r\n\r\n");
        buffer += Src.body;

        buffer.append("\r\n\r\n");

        return buffer;
    }

	//wrote this ages ago no idea how or why it works
    parsed_request parse_http(const request_packet& request) {
        parsed_request http_request;
        std::string buffer(request.data);

        // find method
        int method_end = buffer.find(' ');
        if (method_end != std::string::npos) {
            std::string tmp = buffer.substr(0, method_end);

            // can't really make switch case with strings as that would requre an entire function call
            if (tmp == "GET")  http_request.dest.method = GET;
            else if (tmp == "POST") http_request.dest.method = POST;
        }

        // find path
        int path_start = method_end + 1;
        int path_end = buffer.find(' ', path_start);
        if (path_end != std::string::npos)
            http_request.dest.path = buffer.substr(path_start, path_end - path_start);

        // find cookies (if any)
        int cookie_start = buffer.find("Cookie: ");
        if (cookie_start != std::string::npos) {
            cookie_start += 8; // Skip "Cookie: "
            int cookie_end = buffer.find("\r\n", cookie_start);
            std::string cookies = buffer.substr(cookie_start, cookie_end - cookie_start);

            int separator_pos = cookies.find(';');
            int key_value_separator_pos;
            while (separator_pos != std::string::npos) {
                std::string cookie = cookies.substr(0, separator_pos);
                key_value_separator_pos = cookie.find('=');
                if (key_value_separator_pos != std::string::npos) {
                    std::string key = cookie.substr(0, key_value_separator_pos);
                    std::string value = cookie.substr(key_value_separator_pos + 1);
                    http_request.cookies[key] = value;
                }

                cookies.erase(0, separator_pos + 1);
				separator_pos = cookies.find(';');
            }

			key_value_separator_pos = cookies.find('=');
			if (key_value_separator_pos != std::string::npos) {
				std::string key = cookies.substr(0, key_value_separator_pos);
				std::string value = cookies.substr(key_value_separator_pos + 1);
				http_request.cookies[key] = value;
            }
        }

		int keep_alive_start = buffer.find("Connection: keep-alive");
		if (keep_alive_start != std::string::npos) {
			http_request.keep_alive = true;
		} else {
			http_request.keep_alive = false;
		}

        http_request.fd = request.fd;
        return http_request;
    }

	// done for Readability sakes will, O2 will probably even it out
	enum route_status{
		NOT_IMPLEMENTED = 0,
		IMPLEMENTED = 1
	};		

	class request_router {
	private:
        file_cache cache;
		logging::logger &w_log;
        std::unordered_map<destination, std::function<void(parsed_request&, packet_response&)>> routes;

    public:
		//all logs are performed by request routes e.g
		//Initial connection, request packets sent, tcp disconnect, etc...
        request_router(std::string cache_path, logging::logger &log_ref) :
            cache(cache_path, CACHE_SIZE),
			w_log(log_ref){
        }

        inline void insert(destination path, std::function<void(parsed_request&, packet_response&)> route) {
            routes[path] = route;
        }

		// executed by threadpool contains 90% of all important routing and miscellaneous functionality
        // Note: ugly ass indentaion and horrendus if-else statement(s)
		void execute(request_packet client) {
			std::string src_addr = fetch_ip(client.fd); // public ip address might 
			int packets_sent = 0;
			int status = 0;

			//set fd recv timeout
			struct timeval timeout;
			timeout.tv_sec = CONNECTION_TIMEO;
			timeout.tv_usec = 0;

			// kinda janky but session timeouts are done via recv timeout's so if EWOULDBLOCK is reached the 
			// connection is immedietly terminated
			if (setsockopt(client.fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
				w_log.write(logging::fetch_date_s, " - ", src_addr, " Unable to set connection timeout");
				goto terminate_connection;
			}

			w_log.write(logging::fetch_date_s(), " - ", src_addr, " Connected"); 
			while (1) {	
			    if ((status = _recv(client.fd, client.data, false)) == SOCKET_ERROR) {
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						goto terminate_connection;	
				}	

			    std::cout << "connection status: " << status << std::endl;

                parsed_request request = parse_http(client);
                packet_response response(request.fd);

                std::function<void(parsed_request&, packet_response&)> func = routes[request.dest];
                route_status res = route_stat(func);
                switch(res) {
                    case NOT_IMPLEMENTED: // 501
                        std::cout << "no route found\n" << request.dest.path << "\n path" << request.dest.method << std::endl;

                        response.status = 501;
                        response.body = "Unable to find requested_path";
                        response.set_content_type("text/html");
						response._send(format_http(response));
					
						goto terminate_connection;
                    
                    case IMPLEMENTED:
                        std::cout << "excv interface\n";
                        func(request, response);
                        std::string cache_mem = cache.fetch(response.body); //retrieve cache content
                        long cache_size = cache_mem.size();

					    std::cout << "normal response\n\n\n\n\n\n\n\n";

					    response.body = cache_mem;
                        response.length = cache_mem.size();
                        std::string packet_buffer = format_http(response);
					        
					    std::cout << "sending response: " << response._send(packet_buffer) << std::endl;
					    packets_sent++;
						
						break;
                    }    
                }
			
			terminate_connection:

			std::cout << "closing: " << client.fd << std::endl;
			std::cout << "packet sent before closing: " << packets_sent << std::endl;
            
			if (closesocket(client.fd) == SOCKET_ERROR) {
				#ifdef _WIN32
                    std::cout << WSAGetLastError() << std::endl;
                #endif

                #ifdef __linux__
					throw std::runtime_error("Unable to terminate tcp connection");
                #endif
					
            }
        }

	private:
		inline route_status route_stat(std::function<void(parsed_request&, packet_response&)> func) {
			return (func != nullptr) ? IMPLEMENTED : NOT_IMPLEMENTED;
		}

    };

	class webserver {
	private:
        std::queue<request_packet> clients;
		logging::logger wlog;
        request_router routes;
        nofetch_threadpool pool;

        SOCKET socket_fd;
        sockaddr_in server_addr;
        socklen_t server_len;

        std::thread worker;		// incase user want's to use main thread
        std::atomic<bool> flag;

    public:
        webserver(const char *cache_path, const char *logs_path, int port, int backlog, int thread_count):
        pool(thread_count),
		wlog(std::string(logs_path)),
        routes(std::string(cache_path), wlog){
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
		}

		inline void run() {
			acpt_worker();
		}
		
		inline void async_run() {
			worker = std::thread(&http::webserver::acpt_worker, this);
		}

        void async_shutdown() {
            flag.store(false);
            pool.shutdown();
			if (worker.joinable())
				worker.join();
			
			wlog.shutdown();
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
