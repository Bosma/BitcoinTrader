#include "handlers/okcoin_futs_handler.h"

using std::shared_ptr;
using std::accumulate;
using std::abs;
using std::to_string;
using std::string;
using namespace std::chrono;
using namespace std::chrono_literals;
using std::this_thread::sleep_for;
using std::function;
using std::ostringstream;
using boost::optional;
using std::promise;
using std::future_status;
using std::move;

void OKCoinFutsHandler::manage_positions(double signal) {
  // fetch the current position
  // if something is wrong, restart this function
  auto position = okcoin_futs->positions();
  if (!position)
    return;

  // fetch userinfo, if we cannot just return
  auto userinfo = get_userinfo();
  if (!userinfo)
    return;

  // fetch tick, return if it's stale
  auto t = tick.get();
  if (timestamp_now() - t.timestamp > 30s)
    return;

  // calculate the number of contracts we have open, and the number of contracts we would like to have
  // negative contracts are short contracts
  double equity = userinfo->equity * t.last;
  double max_exposure = position->lever_rate * equity;
  double desired_exposure = signal * max_exposure;;
  int desired_contracts = static_cast<int>(floor(desired_exposure / 100));

  // TODO: remove for live
  if (desired_contracts > 0)
    desired_contracts = 1;
  else if (desired_contracts == 0)
    desired_contracts = 0;
  else
    desired_contracts = -1;

  int current_contracts = position->buy.contracts - position->sell.contracts;

  // number of contracts we have to close and/or open
  int contracts_to_close;
  int contracts_to_open;

  // if the signs are different or if we desire 0 contracts
  // we're closing all the current contracts, and opening
  // all of desired contracts (or 0 if it's 0)
  if ((current_contracts > 0 && desired_contracts <= 0) ||
      (current_contracts < 0 && desired_contracts >= 0)) {
    contracts_to_close = current_contracts;
    contracts_to_open = desired_contracts;
  }
    // if the signs are the same
    // we have to figure out if we're increasing or decreasing our exposure
  else {
    // if we are decreasing our exposure, close the difference
    if (abs(desired_contracts) < abs(current_contracts)) {
      contracts_to_close = current_contracts - desired_contracts;
      contracts_to_open = 0;
    }
      // if we're increasing our exposure, open the difference
    else {
      contracts_to_open = desired_contracts - current_contracts;
      contracts_to_close = 0;
    }
  }

  string action;
  string direction;
  double max_price;

  auto calculations = [&action, &direction, &max_price, &t](auto type) {
    switch (type) {
      case OKCoinFuts::OrderType::OpenLong :
        action = "OPENING"; direction = "LONG";
        max_price = t.bid * (1 + 0.01);
        break;
      case OKCoinFuts::OrderType::OpenShort :
        action = "OPENING"; direction = "SHORT";
        max_price = t.ask * (1 - 0.01);
        break;
      case OKCoinFuts::OrderType::CloseLong :
        action = "CLOSING"; direction = "LONG";
        max_price = t.ask * (1 - 0.01);
        break;
      case OKCoinFuts::OrderType::CloseShort :
        action = "CLOSING"; direction = "SHORT";
        max_price = t.bid * (1 + 0.01);
    }
  };

  // if we have any contracts to close, convert the contracts to long or short contracts instead of negative/positive
  if (contracts_to_close != 0) {
    auto to_close = (contracts_to_close >= 0) ? OKCoinFuts::OrderType::CloseLong : OKCoinFuts::OrderType::CloseShort;
    calculations(to_close);
    trading_log->output("MARKET " + action + " " + to_string(abs(contracts_to_close)) + " " + direction + " CONTRACTS WITH MAX PRICE " + to_string(max_price));
    // limit with price crossing the bid/ask immediately executes with maximum slippage
    if (!limit(to_close, abs(contracts_to_close), position->lever_rate, max_price, 30s)) {
      // if we've failed to close, discontinue
      return;
    }
  }

  // if we have any contracts to open, convert the contracts to long or short contracts instead of negative/positive
  if (contracts_to_open != 0) {
    auto to_open = (contracts_to_open >= 0) ? OKCoinFuts::OrderType::OpenLong : OKCoinFuts::OrderType::OpenShort;
    calculations(to_open);
    trading_log->output("MARKET " + action + " " + to_string(abs(contracts_to_open)) + " " + direction + " CONTRACTS WITH MAX PRICE " + to_string(max_price));
    // no need to check for success, since it's the last thing we do
    // if it fails, manage positions loops again
    limit(to_open, abs(contracts_to_open), position->lever_rate, max_price, 30s);
  }
}
