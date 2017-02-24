#pragma once

#include "../include/exchange_data.h"
#include "../include/okcoin_futs.h"

class OKCoinFutsHandler : public ExchangeHandler {
public:
  OKCoinFutsHandler(std::string, std::shared_ptr<Log>, std::shared_ptr<Config>, OKCoinFuts::ContractType);

  void set_up_and_start() override;

  // TODO: can this can be moved to the ExchangeHandler, if UserInfo is abstracted?
  std::string print_userinfo() override;

  Atomic<OKCoinFuts::UserInfo> user_info;
  std::shared_ptr<OKCoinFuts> okcoin_futs;
  OKCoinFuts::ContractType contract_type;
};