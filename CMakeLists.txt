cmake_minimum_required (VERSION 2.8)
project (BitcoinTrader)

set (CMAKE_CXX_FLAGS "-Wall -std=c++14 -stdlib=libstdc++ -D_WEBSOCKETPP_CPP11_STL_ -DBOOST_ASIO_HAS_STD_CHRONO")

set (CMAKE_EXE_LINKER_FLAGS "-lboost_system -lboost_filesystem -lpthread -lcrypto -lssl -lcurl")

file (GLOB_RECURSE SOURCES "src/*.cpp")

include_directories (include)

include_directories (libs/json)
include_directories (libs/websocketpp)
include_directories (libs/ctpl)

add_executable (BitcoinTrader ${SOURCES})

add_executable (ticker EXCLUDE_FROM_ALL ticker.cpp src/utilities/log.cpp src/websocket/websocket.cpp src/exchanges/okcoin.cpp src/exchanges/okcoin_futs.cpp src/utilities/exchange_utils.cpp src/utilities/curl.cpp)
target_link_libraries(ticker "-lzmq")