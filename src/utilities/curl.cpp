#include "utilities/curl.h"

std::string curl_post(std::string url, std::shared_ptr<Log> log, std::string post_fields) {
  CURL *curl;
  CURLcode res;

  std::string output = "";

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
      log->output("curl_post() ERROR " + std::string(curl_easy_strerror(res)) + ", with: "  + url + ", post_fields: " + post_fields);

    curl_easy_cleanup(curl);
  }
  else
    log->output("curl_post(): curl_easy_init() RETURNED NULL.");

  return output;
}