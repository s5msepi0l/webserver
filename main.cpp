#define LOGGER_ENABLE_AUTO_WRITE 64 // Not neccesary at all, should probably be turned off in prod

#include "src/webserver.h" 
#include "src/logging.h"


int main(int argc, const char **argv) {
	http::webserver server(
		".",
		8001, 
		512, 
		4);
    
	server.get("/", [](
	parsed_request& request, packet_response& response, f_cache &cache){
		response.set_body_content(cache->fetch("index.html"));
		response.set_content_type("text/html");
		response.set_content_status(200);

	});

	server.get("/style.css", [](
	parsed_request& request, packet_response& response, f_cache &cache){
		response.set_body_content(cache->fetch("style.css"));
		response.set_content_type("text/css");
		response.set_content_status(200);

	});

	server.get("/script.js", [](
	parsed_request& request, packet_response& response, f_cache &cache){
		response.set_body_content(cache->fetch("script.js"));
		response.set_content_type("application/javascript");
		response.set_content_status(200);

	});

	server.get("/static/wallpaper.gif", [](
	parsed_request& request, packet_response& response, f_cache &cache){
		response.set_body_content(cache->fetch("static/wallpaper.gif"));
		response.set_content_type("image/gif");
		response.set_content_status(200);

	});

	server.get("/static/github.png", [](
	parsed_request& request, packet_response& response, f_cache &cache){
		response.set_body_content(cache->fetch("static/github.png"));
		response.set_content_type("image/png");
		response.set_content_status(200);

	});

	server.run();

	return 0;
}
