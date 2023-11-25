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

Note: I added a filecache refrence to the interface to resolve some admittedly crappy tech debt
Note: to change (any) of the finner details via macro defenitions you will have to
	define them before you include the header file, macro defenitions are documented in the readme
Note: Http Keep-Alive support is done via threadpool with added load queuing
Note: Refrain from tweaking the networking structs too much as it may cause tech-debt
Note: logs only contain connections information along with the amount of requests sent by the client

TODO:
	- Automatically set http content type
	- Add optional ip-address whitelisting
	- Make JSON implementation less "fucky" to work with
	- Add basic json support for receiving packet and sending
	- Update logging for more customizability and robust functionality
*/


#pragma once

#include "threadpool.h"
#include "cache.h"
#include "logging.h"
#include "json.h"

#include <iostream>
#include <queue>
#include <atomic>
#include <memory>

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

typedef std::shared_ptr<file_cache> f_cache;

inline int _send(SOCKET fd, std::string buffer) {
    return send(fd, buffer.c_str(), buffer.size(), 0);
}

inline int _recv(SOCKET fd, std::string &buffer) {
	char data_buffer[BUFFER_SIZE]{ 0 };
    size_t bytes_recv;
	bytes_recv = recv(fd, data_buffer, BUFFER_SIZE - 1, 0);

	data_buffer[bytes_recv] = '\0'; // as to not fill the entire buffer with the buffer amount
	buffer = data_buffer; //assuming that the buffer param will be empty when passed
    
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
	JSON::json body;
	bool keep_alive;
    destination dest;
    std::unordered_map<std::string, std::string> cookies;
} parsed_request;

typedef struct packet_response {
	SOCKET fd;

    int status;
    std::string content_type;
    size_t length;

    std::string body;
	
	packet_response(SOCKET Dst): fd(Dst), status(200) {}

	// returns the amount of induvidial packets sent to client
	int _send() {
		std::string packet = format_http();

		if (packet.size() <= BUFFER_SIZE) {
			send(fd, packet.c_str(), packet.size(), 0);
			return 1;
		}

		return _send_all();
	}

    inline int _send_all() {
        long cache_size = this->body.size();
        
		std::string status_code;
		switch(this->status) {
			case 200:
				status_code = "OK";
				break;
					
			case 404:
				status_code = "Not Found";
				break;
		}

		std::stringstream res;
        res << "HTTP/1.1 " + (std::to_string(this->status) + " " + status_code + "\r\n")
            << "Content-Type: " << this->content_type << "\r\n"
            << "Transfer-Encoding: chunked\r\n"
            << "\r\n";

        // Send the HTTP headers
        const std::string &response_str = res.str();
        send(this->fd, response_str.c_str(), response_str.length(), 0);

        long bytes_sent = 0;
		int packets_sent = 1;
        while (bytes_sent < cache_size)
        {
            long chunk_size = _min(BUFFER_SIZE, cache_size - bytes_sent);

            std::string chunk_header;
            chunk_header.reserve((chunk_size % 15) + 2);
            chunk_header.append(hexify(chunk_size));
            chunk_header.append("\r\n");
            
			send(this->fd, chunk_header.c_str(), chunk_header.size(), 0);
            send(this->fd, this->body.c_str() + bytes_sent, chunk_size, 0);
            send(this->fd, "\r\n", 2, 0);

			packets_sent++;

            bytes_sent += chunk_size;
        }

        // final packet indicating EOF
        send(this->fd, "0\r\n\r\n", 5, 0);
		return packets_sent;
	}
	
	inline void set_body_content(std::string content) {
		this->body = content;
		this->length = content.size();
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

	std::string format_http() {
		std::string status_code;
		switch(this->status) {
			case 200:
				status_code = "OK";
				break;
					
			case 404:
				status_code = "Not Found";
				break;
		}

		std::stringstream buffer;
        buffer << "HTTP/1.1 " + (std::to_string(this->status) + " " + status_code + "\r\n")
			<< "Content-Type: " + this->content_type + "\r\n"
			<< ("Content-Length: " + std::to_string(this->length) + "\r\n\r\n")
			<< this->body
			<< "\r\n\r\n";

        return buffer.str();
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

	//wrote this ages ago no idea how or why it works
    parsed_request parse_http(const request_packet& request) {
        parsed_request http_request;
        std::string buffer(request.data);

		// creating new stack variables for every header
		//some light compiler optimization will probably just reuse the same stack memory 
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

		int is_json;
		if ((is_json = buffer.find("Content-Type: application/json")) != std::string::npos) {
			std::string json_content = buffer.substr(buffer.find("["), buffer.find("]"));
			http_request.body = JSON::parse(json_content);
		}

        http_request.fd = request.fd;
        return http_request;
    }

	class whitelist {
		private:
			std::unordered_set<std::string> tree;

		public:
			whitelist(std::string path) {
				std::vector<std::string> lst = read_l(path);
				for (int i = 0; i<lst.size(); i++) {
					tree.insert(lst[i]);
				}
			}

			inline void add(std::string Src) {
				tree.insert(Src);
			}

			inline bool auth(std::string val) {
				return (tree.find(val) != tree.end()) ? true : false;
			}
	};

	// done for Readability sakes will, O2 will probably even it out
	enum route_status{
		NOT_IMPLEMENTED = 0,
		IMPLEMENTED = 1
	};		

	class request_router {
	private:
		file_cache cache; //don't use this directly
		logging::logger &w_log;
        std::unordered_map<destination, std::function<void(parsed_request&, packet_response&, f_cache&)>> routes;

    public:
		//all logs are performed by request routes e.g
		//Initial connection, request packets sent, tcp disconnect, etc...
        request_router(std::string cache_path, logging::logger &log_ref) :
            cache(cache_path, CACHE_SIZE),
			w_log(log_ref){
        }

        inline void insert(destination path, std::function<void(parsed_request&, packet_response&, f_cache&)> route) {
            routes[path] = route;
        }

		// executed by threadpool, contains 90% of all important routing and miscellaneous functionality
		void execute(request_packet client) {
			std::shared_ptr<file_cache> cache_ptr = std::make_shared<file_cache>(this->cache); // one must imagine sisyphus happy

			std::string src_addr = fetch_ip(client.fd);
			int packets_sent = 0;
			int status = 0;

			//set fd recv timeout
			struct timeval timeout;
			timeout.tv_sec = CONNECTION_TIMEO;
			timeout.tv_usec = 0;

			// kinda janky but session timeouts are done via recv timeout's so if EWOULDBLOCK is reached the 
			// connection is subsequently terminated
			if (setsockopt(client.fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
				w_log.write('[', logging::fetch_date_s, ']', " - ", "[INFO]", src_addr, "   - ",
						"Unable to set connection timeout");
				goto terminate_connection;
			}

			w_log.write('[', logging::fetch_date_s(), ']', " - ", "[ACTION]", " - ", src_addr, " Connected"); 
			while (1) {	
				status = _recv(client.fd, client.data);
			    if (status == SOCKET_ERROR) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						w_log.write('[', logging::fetch_date_s(), ']', " - ", "[ACTION]", " - ", src_addr, " Session timeout");
						std::cout << "SESSION_TIMEO\n";
						goto terminate_connection;	
					}
				}

				if (status == 0) { // connection terminated
					std::cout << "Connection reset\n";
					w_log.write('[', logging::fetch_date_s(), ']', " - ", "[ACTION]", " - ", src_addr, " Connection closed by peer");
					goto terminate_connection; 
				}

                parsed_request request = parse_http(client);
                packet_response response(request.fd);

				w_log.write('[', logging::fetch_date_s(), ']', " - ", "[INFO]", "   - ", src_addr, " Requested: ", '[', request.dest.path, ']');
	
                std::function<void(parsed_request&, packet_response&, f_cache&)> func = routes[request.dest];
                route_status res = route_stat(func);
                
				switch(res) {
                    case NOT_IMPLEMENTED: // 404
                        std::cout << "no route found\n" << request.dest.path << "\n path" << request.dest.method << std::endl;

                        response.status = 404;
                        response.body = "Unable to find requested_path";
                        response.set_content_type("text/html");
						response._send();
					
						w_log.write('[', logging::fetch_date_s(), ']', " - ", "[INFO]", "   - ", src_addr, " Unable to fetch: ", request.dest.path);
						//goto terminate_connection;
						
						break;

                    case IMPLEMENTED:
						std::cout << "route found!, path: " << request.dest.path << " method: " << request.dest.method << std::endl;
                        func(request, response, cache_ptr);
					    response._send();

					    packets_sent++;
						
						break;
                    }    
                }
			
			terminate_connection:
			std::cout << "\n\n CLOSING CONNECTION: " << client.fd << "\n\n";
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
		inline route_status route_stat(std::function<void(parsed_request&, packet_response&, f_cache&)> func) {
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
        webserver(std::string content_path, int port, int backlog, int thread_count):
        pool(thread_count),
		wlog(content_path + "/logs"),
		routes(content_path + "/site", wlog){
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

        inline void get(std::string path, std::function<void(parsed_request&, packet_response&, f_cache&)> route) {
            destination buf;
            buf.method = GET;
            buf.path = path;

            routes.insert(buf, route);
        }

        inline void post(std::string path, std::function<void(parsed_request&, packet_response&, f_cache&)> route) {
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
