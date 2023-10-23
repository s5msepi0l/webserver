#include "src/webserver.h" 
#include "src/logging.h"

int main(int argc, const char **argv) {
    
	http::webserver server("/home/s5msep1ol/Desktop/projects/webserver/main/site", 8004, 512, 4);
    server.get("/", [](parsed_request& request, packet_response& response) {
		set_body_content("index.html", response);
		set_content_type("text/html", response);
		set_content_status(200, response);

	});


    sleep(120);

    server.shutdown();
    
	return 0;
}
