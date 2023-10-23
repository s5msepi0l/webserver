#pragma once

#include <iostream>
#include <string>
#include <utility>
#include <stack>
#include <vector>
#include <unordered_map>
#include <variant>
#include <string.h>

inline bool is_letter(char src) {
	return (src >= 32 && src <= 126) ? true : false;
}

inline bool is_digit(char src) {
	return (src >= '0' && src <= '9') ? true : false;
}

inline bool m_is_digit(const char *src) {
	return ((*src >= '0' && *src <= '9') || (*(src+1) >= '0' && *(src+1) <= '9')) ? true : false;
}


namespace JSON {
	typedef enum {
		STRING,
		NUMBER,
		BOOLEAN,
		OPERATOR,
		BRACKET
	}lexical_token;
	
	typedef struct {
		lexical_token type;
		std::string value;
	}token;

	void display_token(std::vector<token> &src) {
		std::string type = "";

		for (int i = 0; i < src.size(); i++) {
			switch (src[i].type) {
			case STRING:
				type = "STRING";
				break;

			case NUMBER:
				type = "NUMBER";
				break;

			case BOOLEAN:
				type = "BOOLEAN";
				break;

			case OPERATOR:
				type = "OPERATOR";
				break;

			case BRACKET:
				type = "BRACKET";
				break;
			}
		
			std::cout << "token: (" << i << ")" <<
				"Value: " << '"' <<  src[i].value << '"' <<
				"\nType: " << type << std::endl;
		}
	}

	typedef struct {
		std::vector<std::variant<std::string, std::nullptr_t, bool, int>> value;
	}json;

	void display_json(json src) {
		for (int i = 0; i < src.value.size(); i++) {
			if (std::holds_alternative<std::string>(src.value[i])) {
				std::cout << "json_value: " << std::get<std::string>(src.value[i]) << std::endl;
			}
			else if (std::holds_alternative<bool>(src.value[i])) {
				if (std::get<bool>(src.value[i]) == true)
					std::cout << "json_value: true\n";
				else
					std::cout << "json_value: false\n";
			}
			else if (std::holds_alternative<int>(src.value[i])) {
				int tmp = std::get<int>(src.value[i]);
				std::cout << "json_value: " << tmp << std::endl;
			}
		}
	}
		
	std::vector<token> tokenize(std::string input) {
		std::vector<token> tokens;
		std::stack<char> brackets;
		token buffer;

		for (int i = 0; i < input.size(); i++) {
			switch (input[i]) {
				case ' ':
					continue;

				case '"':
					while (input[++i] != '"') {
						if (is_letter(input[i]))
							buffer.value.push_back(input[i]);
					}
					tokens.push_back(token{ STRING, buffer.value });

					break;

				case ',':
					tokens.push_back(token{ OPERATOR, "," });
						break;

				case '[':
					tokens.push_back(token{ BRACKET, "[" });
					break;

				case ']':
					tokens.push_back(token{ BRACKET, "]" });
					break;

				default: //current value is either boolean or integer
					if (m_is_digit(input.c_str() + i)) { // is integer 
						buffer.value.clear();
						buffer.type = NUMBER;
						while (m_is_digit(input.c_str() + i)) {
							buffer.value.push_back(input[i++]);
						}
						tokens.push_back(buffer);
					
						
					}

					switch (input[i]) {
						case 't':
							if (input.substr(i, 4) == "true") {
								tokens.push_back(token{ BOOLEAN, "true" });
							}
							break;

						case 'f':
							if (input.substr(i, 5) == "false") {
								tokens.push_back(token{ BOOLEAN, "false" });
							}
							break;
					}
			}
		}

		return tokens;
	}

		json parse(std::string input) {
			std::vector<token> tokens = tokenize(input);
			json buffer;
			display_token(tokens);

			for (int i = 0; i < tokens.size(); i++) {
				switch (tokens[i].type) {
					case STRING:
						buffer.value.push_back(tokens[i].value);
						break;

					case BOOLEAN:
						buffer.value.push_back(tokens[i].value);
						break;

					case NUMBER:
						buffer.value.push_back(atoi(tokens[i].value.c_str()));

						break;
				}
					
			}
			
			return buffer;
		}

		std::string stringify(json content) {
			std::string buffer("[");
			for (int i = 0; i < content.value.size(); i++) {
				if (std::holds_alternative<std::string>(content.value[i])) {
					std::string tmp_content("\"");
					tmp_content.append(std::get<std::string>(content.value[i]));
					tmp_content.append("\"");

					buffer.append(tmp_content);
				}
				else if (std::holds_alternative<bool>(content.value[i])) {
					if (std::get<bool>(content.value[i]) == true)
						buffer.append("true");
					else
						buffer.append("false");
				}
				else if (std::holds_alternative<int>(content.value[i])) {
					int tmp_int = std::get<int>(content.value[i]);
					buffer.append(std::to_string(tmp_int));
				}
			
				if (i > content.value.size())
					buffer.append(",");
				else
					buffer.append("]");
			}
			return buffer;
		}
};
