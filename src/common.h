#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

std::string read_f(std::string path) {
	std::ifstream file(path, std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("Unable to open file:");

	std::string buffer;
	unsigned char byte;

	while (file.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
		buffer.push_back(byte);
	}

	file.close();

	return buffer;
}

inline std::string hexify(long val) {
	std::stringstream stream;
	stream << std::hex << val;
	return stream.str();
}
