#include <numeric>
#include "utilities/csv.h"

CSV::CSV(std::string file_name, const std::vector<std::string> &columns) {
  csv_file.open(file_name, std::ofstream::app);
  if (!boost::filesystem::exists(file_name))
    row(columns);
}

void CSV::row(const std::vector<std::string> &columns) {
  csv_file << std::accumulate(std::next(columns.begin()),
                              columns.end(),
                              columns[0],
                              [](std::string a, std::string b) {
                                return a + "," + b;
                              })
           << std::endl;
};