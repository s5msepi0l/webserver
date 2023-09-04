#ifndef CPP_WEBSERVER_CACHE
#define CPP_WEBSERVER_CACHE

#include "common.h"
#include <list>
#include <utility>
#include <unordered_map>

class LRUcache {
private:
    int capacity;
    std::unordered_map<std::string, std::pair<std::string, std::list<std::string>::iterator>> cache; // key -> (value, iterator in list)
    std::list<std::string> list; // Doubly linked list to maintain LRU order

public:
    LRUcache(int cap) : capacity(cap) 
    {}

    inline bool ispresent(const std::string key) {
       return (cache.find(key) != cache.end()) ? true : false;                      
    }

    std::string get(const std::string& key) {
        if (cache.find(key) != cache.end()) {
            // item found in cache, move it to the front of the list
            list.splice(list.begin(), list, cache[key].second);
            return cache[key].first;
        }
        return ""; 
    }

    void put(const std::string key, const std::string value) {
        if (ispresent(key)) return;

        if (cache.size() >= capacity) {
            // cache is full, evict the least recently used item (from the back of the list)
            std::string lruKey = list.back();
            cache.erase(lruKey);
            list.pop_back();
        }

        list.push_front(key);
        cache[key] = std::make_pair(value, list.begin());
    }
};

class file_cache : public LRUcache {    
public:
    file_cache(int cap) : LRUcache(cap) 
    {}

    inline std::string fetch(const std::string &key) {
        return LRUcache::get(key);
    }

    void set(const std::string key, const std::string path) {
        if (!(LRUcache::ispresent(key))) {
            std::string content = n_fetch(path);
            LRUcache::put(key, content);
        }
    }
};

#endif