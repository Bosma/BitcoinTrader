#pragma once

#include <string>
#include <sstream>
#include <map>
#include <chrono>
#include <memory>
#include <fstream>

#include <openssl/md5.h>

#include "../json/json.hpp"

#include "../include/websocket.h"
#include "../include/log.h"
#include "../include/exchange_utils.h"
#include "../include/exchange.h"

class OKCoin : public Exchange {
  public:
    const std::string OKCOIN_URL = "wss://real.okcoin.com:10440/websocket/okcoinapi";
    OKCoin(std::string, std::shared_ptr<Log>, std::shared_ptr<Config>);
    
    // start the websocket
    void start();
    
    // send ping to OKCoin
    void ping();
    // return a string of each channels name and last received message
    std::string status();

  protected:
    std::string api_key;
    std::string secret_key;
    websocket ws;
    
    void on_open();
    virtual void on_message(std::string const &) = 0;
    void on_close();
    void on_fail();
    void on_error(std::string const &);

    // OKCOIN ERROR CODES
    std::map<std::string, std::string> error_reasons;
    void populate_error_reasons();
    
    // DATA AND FUNCTIONS FOR CHANNELS
    std::map<std::string, std::shared_ptr<Channel>> channels;
    void subscribe_to_channel(std::string const &);
    void unsubscribe_to_channel(std::string const &);

    // GET MD5 HASH OF PARAMETERS
    std::string sign(std::string);

    std::string period_conversions(std::chrono::minutes); 
    std::chrono::minutes period_conversions(std::string); 
};
