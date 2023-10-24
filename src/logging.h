/* logging.h
 *
 * logs information via a somewhat basic file rotation system when-
 * ever the current log file reaches a certain size by default ~5 mb (can be changed via
 * preprocessor defenitions)
 * */

#pragma once

#ifndef MAX_LOG_SIZE
#define MAX_LOG_SIZE 5 * 1024 * 1024 // ~5mb, bss (dword)
#endif

#include <iostream> 
#include <fstream>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace logging {
	// return's true if path_old is newer
	bool datecmp(std::string& path_old, std::string& path_new, std::string delim) {
		size_t pos_old = path_old.find(delim);
		size_t pos_new = path_new.find(delim);
		if (pos_new == std::string::npos) {
			return true;
		}

		std::string date_old = path_old.substr(pos_old + delim.length(), 10);
		std::string date_new = path_new.substr(pos_new + delim.length(), 10);

		std::tm tm_old = {};
		strptime(date_old.c_str(), "%Y-%m-%d", &tm_old);
		std::time_t time_old = std::mktime(&tm_old);

		std::tm tm_new = {};
		strptime(date_new.c_str(), "%Y-%m-%d", &tm_new);
		std::time_t time_new = std::mktime(&tm_new);

		return difftime(time_old, time_new) > 0;
	}

	std::string fetch_date() {
		std::time_t now = std::time(nullptr);
		char buf[20];
		std::tm tm;
		localtime_r(&now, &tm);  // using localtime_r for thread-safe behavior

		// format the date as a string
		if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm)) 
			return std::string(buf);
		return "";
	}

	// basically just fetch_date but with more precision
	std::string fetch_date_s() {
		std::time_t now = std::time(nullptr);
		char buf[40];
		std::tm tm;
		localtime_r(&now &tm);

		if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm))
			return std::string(buf);
		return "";
	}

	size_t fetch_size(std::string path) {
		struct stat stat_buf;
		int rc = stat(path.c_str(), &stat_buf);
		return rc == 0 ? static_cast<ssize_t>(stat_buf.st_size) : -1;
	}

	// does somewhat clog upp the main thread if the logs folder
	// is sufficently large enough
	class logger {
		private:
			std::string path;
			std::ofstream fd;
			size_t fd_size; 

		public:
			logger(std::string log_path):
			path(log_path){
				std::string buffer, max;
				max.clear();
				for (const auto &entry : fs::directory_iterator(log_path)) {
					buffer = entry.path();
					if (datecmp(buffer, max, "log_"))
						max = buffer;
				}
				if (max.empty()) { // directory is empty
					fd = std::ofstream( // close old file and create new file
					std::string(path + "/log_" + fetch_date() + ".txt"),
					std::ios::out | std::ios::app );
				}
			}
			
			template <typename... Args>
			void write(Args... args) {
				std::stringstream ss;
				(ss << ... << args);
				log(ss.str());
			}

			~logger() {
				fd.close();
			}



		private:
			// automatically adds a new line char at the end
			inline void log(std::string src) { 
				if (fd_size >= MAX_LOG_SIZE) {
					fd.close();
					fd = std::ofstream( // close old file and create new file
						std::string(path + "/log_" + fetch_date() + ".txt"),
						std::ios::out | std::ios::app );
				}
				
				if (fd.is_open()) {
					fd << std::string(src + '\n');
					fd_size += src.size() + 1;
				}
			}
	};
}
