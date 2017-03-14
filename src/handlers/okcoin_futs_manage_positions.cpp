#include "handlers/okcoin_futs_handler.h"

using std::shared_ptr;
using std::accumulate;
using std::abs;
using std::to_string;
using std::string;
using std::chrono::seconds;
using namespace std::chrono_literals;
using std::this_thread::sleep_for;
using std::function;
using std::ostringstream;
using boost::optional;

void OKCoinFutsHandler::manage_positions(double signal) {
  // fetch the current position
  // if something is wrong, restart this function
  auto position = okcoin_futs->positions();
  if (!position.valid)
    return;

  // fetch userinfo, if we cannot just return
  auto userinfo = okcoin_futs_userinfo();
  if (!userinfo)
    return;

  // fetch tick, return if it's stale
  auto t = tick.get();
  if (timestamp_now() - t.timestamp > 30s)
    return;

  // calculate the number of contracts we have open, and the number of contracts we would like to have
  // negative contracts are short contracts
  double equity = userinfo->equity * t.last;
  double max_exposure = position.lever_rate * equity;
  double desired_exposure = signal * max_exposure;;
  int desired_contracts = static_cast<int>(floor(desired_exposure / 100));

  // TODO: remove for live
  if (desired_contracts > 0)
    desired_contracts = 1;
  else if (desired_contracts == 0)
    desired_contracts = 0;
  else
    desired_contracts = -1;

  int current_contracts = position.buy.contracts - position.sell.contracts;

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

  // if we have any contracts to close, convert the contracts to long or short contracts instead of negative/positive
  if (contracts_to_close != 0) {
    auto to_close = (contracts_to_close >= 0) ? OKCoinFuts::OrderType::CloseLong : OKCoinFuts::OrderType::CloseShort;
    if (!okcoin_futs_market(to_close, abs(contracts_to_close), position.lever_rate)) {
      // if we've failed to close, discontinue
      return;
    }
  }

  // if we have any contracts to open, convert the contracts to long or short contracts instead of negative/positive
  if (contracts_to_open != 0) {
    auto to_close = (contracts_to_open >= 0) ? OKCoinFuts::OrderType::OpenLong : OKCoinFuts::OrderType::OpenShort;
    // no need to check for success, since it's the last thing we do
    // if it fails, manage positions loops again
    okcoin_futs_market(to_close, abs(contracts_to_open), position.lever_rate);
  }
}