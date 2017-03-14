#pragma once

#include <boost/optional.hpp>

#include "handlers/exchange_handler.h"
#include "exchanges/okcoin_futs.h"

class OKCoinFutsHandler : public ExchangeHandler {
public:
  OKCoinFutsHandler(std::string, std::shared_ptr<Config>, std::string, std::string, OKCoinFuts::ContractType);

  void set_up_and_start() override;
  void manage_positions(double) override;

  boost::optional<OKCoinFuts::UserInfo> okcoin_futs_userinfo();
  bool okcoin_futs_market(OKCoinFuts::OrderType, double, int, std::chrono::seconds = std::chrono::seconds(30));

  std::shared_ptr<OKCoinFuts> okcoin_futs;
  OKCoinFuts::ContractType contract_type;
};