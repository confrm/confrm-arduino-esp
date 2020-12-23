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
  uint64_t value_number;
};

std::vector<SimpleJSONElement> simple_json(String str) {

  std::vector<SimpleJSONElement> result;

  size_t ind = 0;

  // Remove new line and tabs
  for (ind = 0; ind < str.length(); ind++) {
    if (str[ind] == '\n')
      str[ind] = ' ';
  }

  // Find start of json...
  for (ind = 0; ind < str.length(); ind++) {
    if (str[ind] == '{')
      break;
  }

  // Weird error... start was at end, or not found
  if (ind >= str.length() - 1)
    return result;

  // Find key - value pairs:
  size_t key_start = -1, key_end = -1, value_start = -1, value_end = -1;
  bool advance = true;
  String num_str = "";
  char last_char = '\0';
  SimpleJSONType value_type;

  while (ind < str.length()) {

    // Advance to non-space
    if (advance == true && str[ind] == ' ') {
      ind++;
      continue;
    }

    // Keys all start with '"'
    if (key_start == -1) {
      if (str[ind] == '{') {
        ind++;
        continue;
      } else if (str[ind] == '"') {
        key_start = ind + 1;
        ind++;
        advance = false;
        continue;
      }
    }

    // Advance to end of key
    if (key_end == -1) {
      if (str[ind] != '"') {
        ind++;
        continue;
      } else {
        key_end = ind;
        ind++;
        advance = true; // Advance to next non-space
        continue;
      }
    }

    // Value could be string or number
    if (value_start == -1) {
      if (str[ind] == ':') {
        ind++;
        advance = true;
        continue;
      } else if (str[ind] == '"') {
        value_start = ind + 1;
        value_type = STRING;
        last_char = '\\';
        ind++;
        advance = false;
        continue;
      } else {
        value_start = ind;
        value_type = NUMBER;
        ind++;
        advance = false;
        continue;
      }
    }

    if (value_end == -1) {
      if (value_type == STRING) {
        if (str[ind] != '"' ||
            last_char ==
                '\\') { // Advancing to end of string, check for escape char
          last_char = str[ind];
          ind++;
          continue;
        } else {
          value_end = ind;
          ind++;
          advance = true;
          continue;
        }
      } else {
        if (str[ind] != ',' && str[ind] != ' ' && str[ind] != '}') {
          ind++;
          continue;
        } else {
          value_end = ind;
          // dont advance ind, leave comma or space for next bit
          continue;
          advance = true;
        }
      }
    }

    if (str[ind] == ',' || str[ind] == '}') {
      SimpleJSONElement kvp;
      kvp.key = str.substring(key_start, key_end);
      kvp.type = value_type;
      kvp.value_string = str.substring(value_start, value_end);
      if (value_type == NUMBER) {
        kvp.value_number = strtoll(kvp.value_string.c_str(), NULL, 0);
      }
      result.push_back(kvp);

      key_start = -1;
      key_end = -1;
      value_start = -1;
      value_end = -1;
      ind++;
      advance = true;
      continue;
    }

    ind++;
  }

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

uint64_t get_simple_json_number(std::vector<SimpleJSONElement> &vect,
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
