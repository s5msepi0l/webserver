//remove in prod
#define LOGGER_ENABLE_AUTO_WRITE 64
#define CONNECTION_TIMEO 300

#include "src/webserver.h" 
#include "src/logging.h"
#include "src/json.h"

int main(int argc, const char **argv) {
	http::webserver server(
		".",
		8004, 
		512, 
		4);



	server.get("/favicon.ico", [](parsed_request &request, packet_response &response, f_cache &cache) {
		response.set_body_content(cache->fetch("favicon.ico"));
		response.set_content_type("image/x-icon");
		response.set_content_status(200);
	});

	server.get("/", [](parsed_request& request, packet_response& response, f_cache &cache) {
		response.set_body_content(cache->fetch("index.html"));
		response.set_content_type("text/html");
		response.set_content_status(200);
	});

	server.get("/style.css", [](parsed_request& request, packet_response& response, f_cache &cache) {
		response.set_body_content(cache->fetch("style.css"));
		response.set_content_type("text/css");
		response.set_content_status(200);
	});

	server.get("/script.js", [](parsed_request& request, packet_response& response, f_cache &cache) {
		response.set_body_content(cache->fetch("script.js"));
		response.set_content_type("application/javascript");
		response.set_content_status(200);
	});

	server.get("/static/github.png", [](parsed_request &request, packet_response &response, f_cache &cache) {
		response.set_body_content(cache->fetch("static/github.png"));
		response.set_content_type("image/x-icon");
		response.set_content_status(200);
	});

	server.get("/static/wallpaper.gif", [](parsed_request &request, packet_response &response, f_cache &cache) {
		response.set_body_content(cache->fetch("static/wallpaper.gif"));
		response.set_content_type("image/x-icon");
		response.set_content_status(200);
	});

	server.post("/num_post", [](parsed_request& request, packet_response& response, f_cache &cache) {
		std::cout << "route /num_post\n";
		int num = 0;
		if (std::holds_alternative<int>(request.body.value[0])) {
			num = std::get<int>(request.body.value[0]) * 2;
		}
		std::cout << "END FETCH DATA\n";
		response.set_body_content (
			"[" + std::to_string(num) + "]"
		);


		response.set_content_type("application/son");
		response.set_content_status(200);
		std::cout << "END ROUTE\n";
	});

	server.run();

	return 0;
}
