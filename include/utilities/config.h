#pragma once

#include <map>
#include <iostream>
#include <fstream>

class Config {
public:
  Config(std::string filename, std::string default_value = "") :
      options(),
      default_value(default_value) {
    // load config
    std::ifstream f(filename);
    if (f) {
      // for each line
      std::string s;
      while (getline(f, s)) {
        // handle comments
        // skip blank lines or ones starting with "
        if (!s.empty() && s.at(0) != '"') {
          // extract key and value, separated by a space
          auto space_location = s.find(' ', 0);
          std::string key = s.substr(0, space_location);
          std::string value = s.substr(space_location + 1);
          options[key] = value;
        }
      }
    }
    else {
      std::cout << "config file bitcointrader.conf required" << std::endl;
    }
  };
  Config() : default_value("") {};

  std::string operator[](std::string key) {
    if (exists(key)) {
      return options[key];
    }
    else {
      return default_value;
    }
  }

  bool exists(std::string key) {
    return (options.count(key) != 0);
  }
private:
  std::map<std::string, std::string> options;
  std::string default_value;
};
