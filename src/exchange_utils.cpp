#include "../include/exchange_utils.h"

long optionally_to_long(nlohmann::json object) {
  if (object.is_string()) {
    return stol(object.get<std::string>());
  }
  else
    return object;
}

double optionally_to_double(nlohmann::json object) {
  if (object.is_string()) {
    return stod(object.get<std::string>());
  }
  else
    return object;
}

int optionally_to_int(nlohmann::json object) {
  if (object.is_string()) {
    return stoi(object.get<std::string>());
  }
  else
    return object;
}

std::string dtos(double n, int digits) {
  std::ostringstream n_ss;
  n_ss << std::fixed << std::setprecision(digits) << n;
  return n_ss.str();
}

// use this function for consistent timestamps
std::chrono::nanoseconds timestamp_now() {
  return std::chrono::high_resolution_clock::now().time_since_epoch();
}
