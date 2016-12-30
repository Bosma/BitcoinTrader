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

template <typename T>
std::string opt_to_string(nlohmann::json object) {
  if (object.is_string()) {
    return object.get<std::string>();
  }
  else
    return std::to_string(object.get<T>());
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
std::chrono::nanoseconds timestamp_now() {
  return std::chrono::high_resolution_clock::now().time_since_epoch();
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

std::string curl_post(std::string url, std::string post_fields) {
  CURL *curl;
  CURLcode res;

  std::string output;
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    if (!post_fields.empty())
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
      curl_easy_strerror(res);

    curl_easy_cleanup(curl);
  } 
  return output;
}
