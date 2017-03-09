#pragma once

#include "handlers/exchange_data.h"
#include "exchanges/okcoin_futs.h"

class OKCoinFutsHandler : public ExchangeHandler {
public:
  OKCoinFutsHandler(std::string, std::shared_ptr<Log>, std::shared_ptr<Config>, OKCoinFuts::ContractType);

  void set_up_and_start() override;

  std::shared_ptr<OKCoinFuts> okcoin_futs;
  OKCoinFuts::ContractType contract_type;
};