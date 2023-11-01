#define LOGGER_ENABLE_AUTO_WRITE 64 // Not neccesary at all, should probably be turned off in prod

#include "src/webserver.h" 
#include "src/logging.h"


int main(int argc, const char **argv) {
	http::webserver server(
		"/home/s5msep1ol/Desktop/projects/webserver/backup_1/site", 
		"/home/s5msep1ol/Desktop/projects/webserver/backup_1/logs",
		8001, 
		512, 
		4);
    
	server.get("/", [](parsed_request& request, packet_response& response) {
		set_body_content("index.html", response);
		set_content_type("text/html", response);
		set_content_status(200, response);

	});

	server.get("/video.mp4", [](parsed_request& request, packet_response& response) {
		set_body_content("video.mp4", response);
		set_content_type("video/mp4", response);
		set_content_status(200, response);
	});


	server.get("/img.jpg", [](parsed_request& request, packet_response& response) {
		set_body_content("img.jpg", response);
		set_content_type("image/jpeg", response);
		set_content_status(200, response);
	});

	sleep(120);

    server.shutdown();

	return 0;
}
