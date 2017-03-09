#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include <sstream>

#include <curl/curl.h>

#include "handlers/bitcointrader.h"

using std::cin;    using std::cout;
using std::string; using std::shared_ptr;
using std::map;    using std::ifstream;
using std::endl;   using std::stringstream;
using std::vector;

int main(int argc, char *argv[]) {
  // requirement of curl libraries
  curl_global_init(CURL_GLOBAL_ALL);

  shared_ptr<Config> config(new Config("bitcointrader.conf"));

  BitcoinTrader trader(config);
  trader.start();

  string input;
  bool done = false;
  while (!done) {
    getline(cin, input);
    if (!input.empty()) {
      stringstream ss(input);
      string buf;
      vector<string> args;
      while (ss >> buf)
        args.push_back(buf);

      if (args[0] == "quit")
        done = true;
      else if (args[0] == "status")
        cout << trader.status();
      else if (args[0] == "csv")
        trader.print_bars();
    }
  }

  return 0;
}
