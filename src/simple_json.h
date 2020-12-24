#ifndef __SIMPLEJSON_H__
#define __SIMPLEJSON_H__

#include <Arduino.h>
#include <cstdlib>
#include <vector>

typedef enum SimpleJSONType { STRING, NUMBER } SimpleJSONType;

struct SimpleJSONElement {
  String key;
  SimpleJSONType type;
  String value_string;
  int64_t value_number;
};

std::vector<SimpleJSONElement> simple_json(String str) {

  /*
   * This JSON decoder is limited to processing only top level elements for key-value pairs which
   * follow a restricted pattern.
   *
   * On any formatting error the output will be garbled or worse.
   *
   * The key must be a simple string contianing only 0-7,a-z,A-Z,_,-
   *
   * The value may be a number (up to int64_t size) or a string follwoing normal JSON string
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

  // Remove new line charachters to make processing easier
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
  bool advance = true; // Used to inidcate advancing over blank space
  char last_char = '\0'; // Used to keep track of previous charachter for escape sequence
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
      if (str[ind] == ':') {
        advance = true;
        continue;
      } else if (str[ind] == '"') {
        value_start = ind + 1;
        value_type = STRING;
        last_char = '\0'; // Init the last_char to something non-escape
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
    // JOSN parenthesis. For strings it will be a double quote. Use the last_char to store
    // the last processed char, when check to see if it is a backquote in order to allow
    // strings to contain escpaed double quotes.
    if (value_end == -1) {
      if (value_type == STRING) {
        if (str[ind] != '"' || last_char == '\\') {
          last_char = str[ind];
          continue;
        } else {
          value_end = ind;
          advance = true;
          was_ending = false;
          continue;
        }
      } else {
        // was_ending is used to tell the next stage that the number value was terminated 
        // using a end of element delimiter (comma or end parenthesis)
        if (str[ind] == ',' || str[ind] == '}') {
          was_ending = true;
          value_end = ind;
          continue;
        } else if (str[ind] == ' ') {
          was_ending = false;
          value_end = ind;
          continue;
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
      kvp.key = str.substring(key_start, key_end);
      kvp.type = value_type;
      kvp.value_string = str.substring(value_start, value_end);
      if (value_type == NUMBER) {
        kvp.value_number = strtoll(kvp.value_string.c_str(), NULL, 0);
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
