#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <boost/filesystem.hpp>

class CSV {
public:
  enum class Mode { Append, Overwrite };
  CSV(std::string, const std::vector<std::string> &, Mode = Mode::Append);
  void row(const std::vector<std::string> &vector);
private:
  std::ofstream csv_file;
  unsigned long n;
};


