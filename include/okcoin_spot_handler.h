#pragma once

#include "../include/exchange_data.h"
#include "../include/okcoin_spot.h"

class OKCoinSpotHandler : public ExchangeHandler {
public:
  OKCoinSpotHandler(std::string, std::shared_ptr<Log>, std::shared_ptr<Config>);

  void set_up_and_start() override;

  // TODO: can this can be moved to the ExchangeHandler, if UserInfo is abstracted?
  std::string print_userinfo() override;

  Atomic<OKCoinSpot::UserInfo> user_info;
  std::shared_ptr<OKCoinSpot> okcoin_spot;
};