/* server.h
* Intermdiate interface between to the network controller to parse http requests, 
* route requests, send response, etc
*/

#ifndef CPP_WEBSERVER_SERVER
#define CPP_WEBSERVER_SERVER

#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <functional>

#include "networking.h"
#include "threads.h"
#include "common.h"

namespace http {
    /* routes requests and contains thread execution
    * like 90% of all business logic is gonna happen here
    */ 
    class request_container {
    public:
        std::map<destination, std::function<void(const parsed_request &, packet_response &)>> routes;

        void route_request(const request_packet unparsed_request) {
            const parsed_request request = parse_http(unparsed_request);
            std::function<void(const parsed_request &, packet_response &)> route = routes[request.dest];            
        
            packet_response *response = new(std::nothrow) packet_response;
            route(request, *response);

            
        }
    };

    class server { // main http-server interface
        private:
            networking::whitelist w_list;
            networking::network_controller controller;

            request_container r_container; 

        public:
            server(const char *w_list_path, unsigned short web_port, unsigned short client_port):
            controller(-1, web_port), 
            w_list(w_list_path)
            {}

            void run() {
                request_packet cli_buffer;
                while (true) {
                    std::cout << "fetch_front\n";
                    cli_buffer = controller.recv_packet();
                    std::cout << "cli_data" << cli_buffer.data << std::endl;

                    std::string buffer = "HTTP/1.1 200 OK\r\n\r\n";
                    buffer.append("Hello world");
                    buffer.append("\r\n\r\n");

                    controller.send_packet(cli_buffer, buffer.c_str(), buffer.size());
                }
            }

            void get(std::string path, std::function<void(const parsed_request &, packet_response &)> func) {
                destination tmp = {GET, path};
                r_container.routes[tmp] = func;
            }
    
            void post(std::string path, std::function<void(const parsed_request &, packet_response &)> func) {
                destination tmp = {POST, path};
                r_container.routes[tmp] = func;
            }
    };
};

#endif