#include "server.h"

int main(int argc, const char **argv) {
  
  http::server webserver(
  "/home/s5msep1ol/Desktop/webserver/cpp_webserver/whitelist.txt",
  9001, 
  6014);

  webserver.run();

  //std::cout << n_fetch("/home/s5msep1ol/Desktop/webserver/py_lib/templates/index.html") << std::endl;
	return 0;
}