#pragma once

#include "handlers/exchange_data.h"
#include "exchanges/okcoin_spot.h"

class OKCoinSpotHandler : public ExchangeHandler {
public:
  OKCoinSpotHandler(std::string, std::shared_ptr<Log>, std::shared_ptr<Config>);

  void set_up_and_start() override;

  std::shared_ptr<OKCoinSpot> okcoin_spot;
};