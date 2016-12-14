cmake_minimum_required (VERSION 3.0.1)
project (BitcoinTrader)

set (CMAKE_CXX_FLAGS "-Wall -std=c++14 -D_WEBSOCKETPP_CPP11_STL_ -DBOOST_ASIO_HAS_STD_CHRONO")

set (CMAKE_EXE_LINKER_FLAGS "-lboost_system -lpthread -lcrypto -lssl -lcurl")

include_directories (include)

file (GLOB SOURCES "src/*.cpp")

add_executable (BitcoinTrader ${SOURCES})
