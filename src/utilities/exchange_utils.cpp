#include "utilities/exchange_utils.h"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::function;
using std::this_thread::sleep_for;
using namespace std::chrono_literals;

// Repeatedly call test() every time_between_checks until stop_time
// If stop_time is 0 it will check test() forever
// Returns true if test() is evaluated to be true before stop_time
// IE: returns TRUE if test()
// If stop_time is reached before test() is true, returns false
// IE: returns FALSE if we have timed out
bool check_until(std::function<bool()> test, timestamp_t stop_time, std::chrono::milliseconds time_between_checks) {
  bool complete = false;
  bool completed_on_time = true;
  do {
    // if we're over time (and our stop_time isn't 0)
    if (timestamp_now() > stop_time &&
        stop_time != std::chrono::high_resolution_clock::time_point{}) {
      completed_on_time = false;
      complete = true;
    }
      // if we're under time (or our stop_time is 0)
    else {
      if (test())
        complete = true;
      else
        std::this_thread::sleep_for(time_between_checks);
    }
  } while (!complete);

  return completed_on_time;
}

std::string dtos(double n, int digits) {
  std::ostringstream n_ss;
  n_ss << std::fixed << std::setprecision(digits) << n;
  return n_ss.str();
}

double truncate_to(double to_round, int digits) {
  int i = to_round * std::pow(10, digits);
  double to_return = i / std::pow(10, digits);
  return to_return;
}

// use this function for consistent timestamps
timestamp_t timestamp_now() {
  return std::chrono::high_resolution_clock::now();
}

size_t Curl_write_callback(void *contents, size_t size, size_t nmemb, std::string *s)
{
  size_t newLength = size*nmemb;
  size_t oldLength = s->size();

  try {
    s->resize(oldLength + newLength);
  }
  catch(std::bad_alloc &e) {
    return 0;
  }

  std::copy((char*) contents,
            (char*) contents + newLength,
            s->begin() + oldLength);

  return size*nmemb;
}

std::string ts_to_string(timestamp_t timestamp) {
  std::time_t rawtime = std::chrono::high_resolution_clock::to_time_t(timestamp);
  std::tm* timeinfo;
  char buffer[80];
  timeinfo = std::localtime(&rawtime);
  std::strftime(buffer, 80, "%F %T", timeinfo);
  return std::string(buffer);
}