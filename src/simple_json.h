#ifndef __SIMPLEJSON_H__
#define __SIMPLEJSON_H__

#ifndef CPP_STANDARD
#include <Arduino.h>
#else
#define String std::string
#endif

#include <cstdlib>
#include <vector>

typedef enum SimpleJSONType { STRING, BOOLEAN, NUMBER } SimpleJSONType;

struct SimpleJSONElement {
  String key;
  SimpleJSONType type;
  String value_string;
  int64_t value_number;
  bool value_boolean;
};

String trim(const String &s)
{
  String newstr = "";
	if (s.length() == 0) return newstr;
	char *begin = (char*)s.c_str();
	char *end = begin + s.length() - 1;
	while (isspace(*begin) && begin < end) begin++;
	while (isspace(*end) && end >= begin) end--;
  for (char* i = begin; i <= end; i++)
    newstr += *begin++;
  return newstr;
}

bool to_bool(String str) {
  String lowerStr = "";
  char *str_c = (char*)str.c_str();
  for (size_t i = 0; i < str.length(); i++) {
    lowerStr += tolower(str_c[i]);
  }
  if (lowerStr == "true") {
    return true;
  } else {
    return false;
  }
}

std::vector<SimpleJSONElement> simple_json(String str) {

  /*
   * This JSON decoder is limited to processing only top level elements for key-value pairs which
   * follow a restricted pattern.
   *
   * On any formatting error the output will be garbled or worse.
   *
   * The key must be a simple string containing only 0-7,a-z,A-Z,_,-
   *
   * The value may be a number (up to int64_t size) or a string following normal JSON string
   * requirements
   *
   * i.e:
   *
   * {
   *   "simple_key": "Some long string with % chars and \" quote marks",
   *   "another-key": 999,
   *   "a_final_key": -2
   * }
   */

  std::vector<SimpleJSONElement> result;

  size_t ind = 0;

  // Remove new line characters to make processing easier
  for (ind = 0; ind < str.length(); ind++) {
    if (str[ind] == '\n')
      str[ind] = ' ';
  }

  // Find start of json (expect this to be str[0]
  for (ind = 0; ind < str.length(); ind++) {
    if (str[ind] == '{')
      break;
  }

  // No start was found
  if (ind >= str.length() - 1)
    throw "Invalid JSON file";

  /*
   * Will look for start and end of each key value pair, processing the JSON string one
   * character at a time.
   */
  
  size_t key_start = -1, key_end = -1, value_start = -1, value_end = -1;
  bool advance = true; // Used to indicate advancing over blank space
  char last_char = '\0'; // Used to keep track of previous character for escape sequence
  bool was_ending = false;
  SimpleJSONType value_type; // Used to track the detected number type

  // Iterating over the length of the string
  for (; ind < str.length(); ind++) {
      
    // Advance to non-space
    if (advance == true && str[ind] == ' ') {
      continue;
    }

    // Keys all start with '"'
    if (key_start == -1) {
      if (str[ind] == '{') {
        continue;
      } else if (str[ind] == '"') {
        key_start = ind + 1;
        advance = false;
        continue;
      }
    }

    // Advance to double quote at the end of key
    if (key_end == -1) {
      if (str[ind] != '"') {
        continue;
      } else {
        key_end = ind;
        advance = true;
        continue;
      }
    }

    // Once white space has been advanced, look for colon delimiter, then look for a
    // double quite (indicating a string), otherwise treat as a number
    if (value_start == -1) {
      last_char = '\0'; // Init the last_char to something non-escape
      if (str[ind] == ':') {
        advance = true;
        continue;
      } else if (str[ind] == '"') {
        value_start = ind + 1;
        value_type = STRING;
        advance = false;
        continue;
      } else if (tolower(str[ind]) == 'f' || tolower(str[ind]) == 't') {
        value_start = ind;
        value_type = BOOLEAN;
        advance = false;
        continue;
      } else {
        value_start = ind;
        value_type = NUMBER;
        advance = false;
        continue;
      }
    }

    // Look for the end of the value part, for numbers this will be a comma or the end of
    // JSON parenthesis. For strings it will be a double quote. Use the last_char to store
    // the last processed char, when check to see if it is a backslash in order to allow
    // strings to contain escaped double quotes.
    if (value_end == -1) {
      if (value_type == STRING) {
        if (str[ind] != '"' || (str[ind] == '"' && last_char == '\\')) {
          last_char = str[ind];
          continue;
        } else {
          value_end = ind;
          advance = true;
          was_ending = false;
          continue;
        }
      } else if (value_type == BOOLEAN) {
        if (str[ind] == ',' || str[ind] == '}') {
          was_ending = true;
          value_end = ind;
          advance = false;
        } else if (str[ind] == ' ') {
          was_ending = false;
          value_end = ind;
          advance = false;
        } else {
          advance = true;
          continue;
        }
      } else {
        // was_ending is used to tell the next stage that the number value was terminated 
        // using a end of element delimiter (comma or end parenthesis)
        // When value_end is found, do not continue. Allow it to fall through to kvp processing
        if (str[ind] == ',' || str[ind] == '}') {
          was_ending = true;
          value_end = ind;
          advance = false;
        } else if (str[ind] == ' ') {
          was_ending = false;
          value_end = ind;
          advance = false;
        } else {
          advance = true;
          continue;
        }
      }
    }

    // Look for the end of this element (could be end of json) unless it has already been detected
    if (was_ending || str[ind] == ',' || str[ind] == '}') {

      // Extract and store data in the results vector
      SimpleJSONElement kvp;
#ifdef CPP_STANDARD
      kvp.key = str.substr(key_start, key_end-key_start);
      kvp.value_string = str.substr(value_start, value_end-value_start);
#else
      kvp.key = str.substring(key_start, key_end);
      kvp.value_string = str.substring(value_start, value_end);
#endif
      kvp.type = value_type;
      if (value_type == NUMBER) {
        kvp.value_number = strtoll(kvp.value_string.c_str(), NULL, 0);
      } else if (value_type == BOOLEAN) {
        kvp.value_boolean = to_bool(trim(kvp.value_string));
      }
      result.push_back(kvp);

      // Reset state variables
      was_ending = false;
      key_start = -1;
      key_end = -1;
      value_start = -1;
      value_end = -1;
      advance = true;

      continue;
    }

  }

  // Fin
  return result;
}

String get_simple_json_string(std::vector<SimpleJSONElement> &vect,
                              String key) {
  for (std::vector<SimpleJSONElement>::iterator it = vect.begin();
       it != vect.end(); ++it) {
    if (it->key == key) {
      return it->value_string;
    }
  }
  return ""; // Raise exception
}

int64_t get_simple_json_number(std::vector<SimpleJSONElement> &vect,
                                String key) {
  for (std::vector<SimpleJSONElement>::iterator it = vect.begin();
       it != vect.end(); ++it) {
    if (it->key == key) {
      return it->value_number;
    }
  }
  return 0; // Raise exception
}

#endif
