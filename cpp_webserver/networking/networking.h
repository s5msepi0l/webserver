/* networking.h
 *
 * networking abtractions e.g network handling threading
 * non I/O blocking for multiple client handling,
 * client handling is done via an interface
 * when an buffered packet is detected the thread passes it to the
 * owner for it to be received and handled (if at all)
 * 
 * TODO:
 *  -thread handling
 *  -client handling e.g: add, delete, get
 *  -incoming client whitelisting
 */

#ifndef NETWORKING_GUARD
#define NETWORKING_GUARD

#define BUFFER_SIZE 8192

#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>  // check filedescriptor for any new activity | fuck you 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>
#include <cassert>

#include <atomic> // used for checking thread execution between variables
#include <thread>
#include <mutex>
#include <condition_variable>

#include <unordered_set> // hashset for whitelisting
#include <vector> // holding file descriptor(s)
#include <map>
#include <queue>

#include "../common.h"
#include "../threading/threadpool.h"

namespace networking {
  void send_packet(parsed_request packet) {
    /* TODO: 
    *      translate struct attributes to string to send furthur to client
    */   
  }

  class whitelist {
  private:
    std::unordered_set<std::string> list;

  public: 
    whitelist(std::string path) { // populate hashset and whatnot
      std::vector<std::string> buffer = l_fetch(path);
      for (int i = 0; i<buffer.size(); i++) {
        list.insert(buffer[i]);
      }
    }  
    
    int auth(const char *Src) {
      return (list.find(Src) != list.end()); // goofy ass c# christian ptr arithmatic
    }

    void remove(const char *Src) {
      list.erase(Src);
    }

    int display() { //displays current whitelist
      int size = list.size(); // not really gonna change register usage but looks nicer
      for (std::string element: list) {
        std::cout << element << std::endl;
      }

      return list.size();
    } 

    inline int size() {
      return list.size();
    } 
  };

  class service_client {
    private:
      int socket_fd;
      struct sockaddr_in server_addr;
      socklen_t addrlen;

    public:
    service_client(int port) {
      socket_fd = socket(AF_INET, SOCK_STREAM, 0);
      
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(port);
      server_addr.sin_addr.s_addr = INADDR_ANY;
      addrlen = sizeof(server_addr);

      int status = 0;
      if ((status = bind(socket_fd, (struct sockaddr*)&server_addr, addrlen)) < 0)
        throw std::runtime_error("Error: " + std::to_string(status));
      
      listen(socket_fd, 1);
    }

    ~service_client() {
      std::cout << "destructor\n";
      close(socket_fd);
    }

    inline int accept_client() { //basic system call and compile-time casting
      socket_fd = accept(socket_fd, (struct sockaddr*)&server_addr, &addrlen);
      return 0;
    }

    int send_buf(const char *Src, size_t len) { // assumes caller sends entire str length
      send(socket_fd, Src, std::strlen(Src), 0);
      return 0;
    }

    std::string recv_buf() {
      char buf[BUFFER_SIZE];
      recv(socket_fd, buf, BUFFER_SIZE-1, 0);

      return std::string(buf);
    }
  };

  class network_controller {
    private:
      int socket_fd;  //used for accepting client(s)
      struct sockaddr_in server_addr;
      socklen_t addrlen;

      unsigned long client_id;

      nofetch_threadpool acpt_t;
      std::atomic<bool> thread_flag;

      std::mutex cv_m;
      std::condition_variable cv;

    public:
      std::vector<request_packet> clients; // holds active clients along with most recent request

      network_controller(int backlog, int port): 
          thread_flag(true),
          acpt_t(1){
        assert(port <= uint_16_limit && port > 0);
        client_id = 0;
        
        socket_fd = socket(AF_INET, SOCK_STREAM, 0); // TCP host fd
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;
        addrlen = sizeof(server_addr);

        assert(bind(socket_fd, (struct sockaddr*)&server_addr, addrlen) != -1);
        assert(listen(socket_fd, backlog) != -1); 
      
        acpt_t.exec([this](){networking::network_controller::async_acpt();});
      }

      ~network_controller() {
        acpt_t.shutdown();

        for (int i = 0; i<clients.size(); i++) {
          close(clients[i].fd);
        }
      }

      request_packet recv_packet() {
        {
          std::unique_lock<std::mutex> lock(cv_m);
          cv.wait(lock, [this](){return !clients.empty();});
        }

        request_packet buffer = clients.front();
        clients.erase(clients.begin());
        return buffer;
      } 
  
      //finds vector index via a iterative binary search solution
      int send_packet(request_packet &Dst, const char *Data, size_t len) {
        if (send(Dst.fd, Data, len, 0) < 0)
          return -1;

        close(Dst.fd);
        return 0;
      }

      int fetch_index(request_packet &Dst) {
          int l = 0;
          int r = client_id;

          while (r >= l) {
              int mid = l + (r - l) / 2;

              if (clients[mid].id == Dst.id) {
                return mid;
              }

              if (clients[mid].id > Dst.id)
                  r = mid - 1;
              else
                  l = mid + 1;
          }

          return -1;
    }

    private:
      void async_acpt() { // accepts clients then sets pollin in 

        int tmp = 0;
        while (thread_flag.load()) {
          request_packet socket_buffer; 
 
          char buffer[BUFFER_SIZE];
          std::memset(buffer, 0, sizeof(buffer));
 
          tmp = accept(socket_fd, (struct sockaddr*)&server_addr, &addrlen);
          recv(tmp, buffer, BUFFER_SIZE-1, 0);

          socket_buffer.fd = tmp;
          socket_buffer.id = client_id++;          
          socket_buffer.data = buffer;

          clients.push_back(socket_buffer);
          cv.notify_all();
        }
      }
    };
};
#endif