# BitcoinTrader
C++ OKCoin spot CNY (multiple exchange support in the future) algo trader.

## Dependencies
1. C++14 compiler (only tested on clang++)
2. Boost
3. libcurl (development library)
4. curl (cli - optional - used for sending emails)

## Build Instructions
```bash
git clone https://github.com/Bosma/BitcoinTrader.git && cd BitcoinTrader
git submodule update --init --recursive
mkdir build && cd build
ln -s ../okcoin_error_reasons.txt ./
cp ../bitcointrader.conf ./
# add API keys to bitcointrader.conf
vim bitcointrader.conf
cmake .. && make
./BitcoinTrader
```

## Usage
./BitcoinTrader will run the strategies created in src/exchange_handler.cpp.

A sample strategy is given in src/strategy.cpp and include/strategy.h.

Define what happens when a strategy sends a long or short signal in the callbacks given to the strategy in src/exchange_handler.cpp constructor. Most likely the callbacks will call an execution algorithm (src/execution_algorithms.cpp) and set limits to position sizes, etc.
