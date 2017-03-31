cmake_minimum_required (VERSION 2.8)
project (BitcoinTrader)

set (CMAKE_CXX_STANDARD 14)

set (CMAKE_CXX_FLAGS "-stdlib=libstdc++ -Wall -D_WEBSOCKETPP_CPP11_STL_ -DBOOST_ASIO_HAS_STD_CHRONO")

file (GLOB_RECURSE SOURCES "src/*.cpp")

# threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# boost
find_package(Boost REQUIRED COMPONENTS system filesystem)

# openssl
find_library(SSL ssl REQUIRED)

# libcurl
find_library(CURL curl REQUIRED)

# crypto
find_library(CRYPTO crypto REQUIRED)

include_directories (include)

include_directories (libs/json)
include_directories (libs/websocketpp)
include_directories (libs/ctpl)

add_executable (BitcoinTrader ${SOURCES})

target_link_libraries(BitcoinTrader Threads::Threads ${SSL} ${CURL} ${CRYPTO} ${Boost_LIBRARIES})

add_executable (ticker EXCLUDE_FROM_ALL ticker.cpp src/utilities/log.cpp src/websocket/websocket.cpp src/exchanges/okcoin.cpp src/exchanges/okcoin_futs.cpp src/utilities/exchange_utils.cpp src/utilities/curl.cpp)
target_link_libraries(ticker "-lzmq")