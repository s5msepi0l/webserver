#pragma once

#include <iostream> // testing purposes (remove in final release)

#include <list>
#include <unordered_map>
#include "common.h"

class LRU_cache {
public:
	LRU_cache(int n) : capacity(n) 
	{}

	std::string get(std::string key) {
		if (cache_map.find(key) != cache_map.end()) {
			move_head(key);
			return cache_map[key]->second;
		}
		
		return "";
	}

	void put(std::string key, std::string value) {
		if (cache_map.find(key) != cache_map.end()) {
			move_head(key);

			return;
		}

		if (cache_nodes.size() >= capacity) {
			std::string tailkey = cache_nodes.back().first;
			cache_nodes.pop_back();
			cache_map.erase(tailkey);
		}

		cache_nodes.push_front({ key, value });
		cache_map[key] = cache_nodes.begin();
	}

private:
	int capacity;
	std::list<std::pair<std::string, std::string>> cache_nodes;
	std::unordered_map < std::string, std::list<std::pair<std::string, std::string>>::iterator> cache_map;

	void move_head(std::string key) {
		auto val = cache_map[key];
		cache_nodes.push_front({ key, val->second });
		cache_nodes.erase(val);
		cache_map[key] = cache_nodes.begin();
	}
};

class file_cache {
private:
	LRU_cache cache;
	std::string source_path;

public:
	file_cache(std::string Src, int cache_size):
		source_path(Src),
		cache(cache_size) {}

	std::string fetch(std::string file_path) {
		std::string buffer;

		if ((buffer = cache.get(file_path)) == "") {
			#ifdef _WIN32 //windows  systems like the path formatted this way for some reason
			std::string tmp = source_path + "\\" + file_path;
			#endif

			#ifdef __linux
			std::string tmp = source_path + "/" + file_path;
			#endif

			buffer = read_f(tmp);
			cache.put(tmp, buffer);
		}

		return buffer;
	}
};
