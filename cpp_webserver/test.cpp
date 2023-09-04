#include <iostream>
#include "cache.h"

int main() {
  
    std::cout << "Hello world" << std::endl;
  
    // Create an LRU cache with a capacity of 2
    LRUcache cache(2);

    // Test the cache operations
    cache.put("key1", "value1");
    cache.put("key2", "value2");

    std::cout << cache.get("key1") << std::endl; // Output: "value1"
    cache.put("key3", "value3"); // Evicts "key2", which is the least recently used
    std::cout << cache.get("key2") << std::endl; // Output: "" (key not found)
    cache.put("key4", "value4"); // Evicts "key1"
    std::cout << cache.get("key1") << std::endl; // Output: "" (key not found)
    std::cout << cache.get("key3") << std::endl; // Output: "value3"
    std::cout << cache.get("key4") << std::endl; // Output: "value4"

    return 0;
}