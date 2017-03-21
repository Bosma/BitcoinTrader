#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <boost/filesystem.hpp>

class CSV {
public:
  CSV(std::string, const std::vector<std::string> &);
  void row(const std::vector<std::string> &vector);
private:
  std::ofstream csv_file;
  unsigned long n;
};


