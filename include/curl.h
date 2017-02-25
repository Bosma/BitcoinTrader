#pragma once

#include <string>
#include <memory>

#include <curl/curl.h>

#include "../include/log.h"

std::string curl_post(std::string, std::shared_ptr<Log>, std::string = "");