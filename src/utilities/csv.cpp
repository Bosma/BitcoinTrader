#include <numeric>
#include "utilities/csv.h"

CSV::CSV(std::string file_name, const std::vector<std::string> &columns, Mode mode) :
    n(columns.size()) {
  csv_file.open(file_name,
                (mode == Mode::Append) ? std::ofstream::app : std::ofstream::out);
  if (boost::filesystem::is_empty(file_name))
    row(columns);
}

void CSV::row(const std::vector<std::string> &columns) {
  // TODO: use a templated tuple instead of runtime checking n
  if (columns.size() == n) {
    csv_file << std::accumulate(std::next(columns.begin()),
                                columns.end(),
                                columns[0],
                                [](std::string a, std::string b) {
                                  return a + "," + b;
                                })
             << std::endl;
  }
  else {
    throw std::runtime_error("row column length not equal to header column length");
  }
};