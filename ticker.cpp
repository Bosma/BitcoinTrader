#include <iostream>
#include <chrono>
#include <thread>

#include <zmq.hpp>

#include "exchanges/okcoin_futs.h"

int main() {
  auto log = std::make_shared<Log>();
  auto config = std::make_shared<Config>();

  std::shared_ptr<OKCoinFuts> okcoin;

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_PUB);

  socket.connect("tcp://127.0.0.1:5555");

  auto set_up_and_start = [&]() {
    okcoin = std::make_shared<OKCoinFuts>("ticker", OKCoinFuts::ContractType::Weekly, log, config);

    okcoin->set_open_callback([okcoin]() {
      okcoin->subscribe_to_ticker();
    });
    okcoin->set_ticker_callback([&](const Ticker &tick) {
      std::string x = std::to_string((tick.ask + tick.bid) / 2);

      zmq::message_t message(x.size());
      memcpy(message.data(), x.data(), x.size());

      socket.send(message);
    });

    okcoin->start();
  };

  set_up_and_start();

  bool warm_up = true;
  while (true) {
    if (warm_up) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
      warm_up = false;
    }
    if (timestamp_now() - okcoin->ts_since_last > std::chrono::minutes(1) ||
        !okcoin->connected()) {
        log->output("RECONNECTING TO " + okcoin->name);
        set_up_and_start();
        warm_up = true;
      }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

}