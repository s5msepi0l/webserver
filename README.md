# cppWebserver
# Author: Jacob Lindborg

<h1> C++ webserver framework with request routing simular to pyFlask</h1>

```cpp
#include "src/webserver.h"

int main(int argc, const char **argv) {
	http::webserver server(
		".",
		8001, 
		512, 
		4);
    
	server.get("/", [](parsed_request& request, packet_response& response, f_cache &cache) {
		response.set_body_content(cache->fetch("index.html"));
		response.set_content_type("text/html");
		response.set_content_status(200);
	});

	server.run();

	return 0;
}
```

<h1> Features </h1>

<ul>
  <li> Keep-alive support </li>
  <li> In house json implementation</li>
  <li> Automatic ip and activity logger </li>
  <li> Non main thread blocking so you can use it on top of other application's </li>
  <li> Automatic static content caching and retrival </li>
  <li> File caching for enhanced responsivness </li>
</ul>

<h1> preprocessor macros documentation </h1>

<ul> 
  <li> 
    <h2> BUFFER_SIZE </h2>
    <p> Can be used to change the packet size when iteracting with a client</p>
    
```cpp
    #define BUFFER_SIZE 1024
    #include "webserver.h"
```
    
  </li>

  <li> 
    <h2> CACHE_SIZE </h2>
    <p> Can be used to change the memory slots allocated for the inbuild LRU file cache</p>
    
```cpp
    #define CACHE_SIZE 72
    #include "webserver.h"
```
    
  </li>
  
  <li> 
    <h2> CONNECTION_TIMEO </h2>
    <p> Can be used to change the session timeout limit </p>

```cpp
    
    #define CONNECTION_TIMEO 300 //5 minutes
    #include "webserver.h"
```
    
  </li>

  <li> 
    <h2> LOGGER_MAX_LOG_SIZE </h2>
    <p> 
      Files logged via the inbuild logger need to create a new file once it reaches a certain size to improve long term performance.
      Using this macro will enable you to customize that limit
    </p>

```cpp
    
    #define LOGGER_MAX_LOG_SIZE 5 * 1024 * 1024 // ~5mb
    #include "webserver.h"
```
    
  </li>

  <li> 
    <h2> LOGGER_ENABLE_AUTO_WRITE </h2>
    <p> 
      DO NOT USE <br>
      This is used in testing and WILL reduce performace as this macro causes the program to close and reopen the log file to save the appended content.
      By default this is set to "LOGGER_MAX_LOG_SIZE", another macro used to change log file sizes
    </p>

```cpp
    
    #define LOGGER_ENABLE_AUTO_WRITE LOGGER_MAX_LOG_SIZE 
    #include "webserver.h"
```
    
  </li>
  
</ul>
