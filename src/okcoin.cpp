#include "../include/okcoin.h"

using std::shared_ptr;
using std::bind;
using std::ifstream;
using std::string;
using std::ostringstream;
using std::endl;
using std::chrono::minutes;

OKCoin::OKCoin(string name, shared_ptr<Log> log, shared_ptr<Config> config) :
  Exchange(name, log, config),
  api_key((*config)["okcoin_apikey"]),
  secret_key((*config)["okcoin_secretkey"]),
  ws(OKCOIN_URL),
  error_reasons() {
  ws.set_open_callback( bind(&OKCoin::on_open, this) );
  ws.set_message_callback( bind(&OKCoin::on_message, this, std::placeholders::_1) );
  ws.set_close_callback( bind(&OKCoin::on_close, this) );
  ws.set_fail_callback( bind(&OKCoin::on_fail, this) );
  ws.set_error_callback( bind(&OKCoin::on_error, this, std::placeholders::_1) );

  populate_error_reasons();
}

void OKCoin::start() {
  ws.connect();
}

void OKCoin::on_open() {
  log->output("OPENED SOCKET to " + ws.get_uri());
  
  // if we're open, no need to reconnect
  reconnect = false;

  if (open_callback)
    open_callback();
}

void OKCoin::on_close() {
  log->output("CLOSE with reason: " + ws.get_error_reason());

  // 1001 is normal close
  if (ws.get_close_code() != 1001) {
    log->output("WS ABNORMAL CLOSE CODE");
    reconnect = true;
  }
}

void OKCoin::on_fail() {
  log->output("FAIL with error: " + ws.get_error_reason());
  reconnect = true;
}

void OKCoin::on_error(string const & error_message) {
  log->output("ERROR with message: " + error_message);
  reconnect = true;
}

void OKCoin::ping() {
  ws.send("{'event':'ping'}");
}

string OKCoin::status() {
  ostringstream ss;
  for (auto chan : channels) {
    auto c = chan.second;
    ss << c->name << " (" << c->status << "): " << c->last_message << endl;
  }
  return ss.str();
}

void OKCoin::populate_error_reasons() {
  ifstream f("okcoin_error_reasons.txt");
  if (f) {
    // for each line
    string s;
    while (getline(f, s)) {
      // extract key and value, separated by a space
      auto space_location = s.find(' ', 0);
      string key = s.substr(0, space_location);
      string value = s.substr(space_location + 1);
      error_reasons[key] = value;
    }
  }
  else {
    log->output("no error reasons file given");
  }
}

void OKCoin::subscribe_to_channel(string const & channel) {
  log->output("SUBSCRIBING TO " + channel);

  ws.send("{'event':'addChannel','channel':'" + channel + "'}");

  shared_ptr<Channel> chan(new Channel(channel, "subscribing"));
  channels[channel] = chan;
}

void OKCoin::unsubscribe_to_channel(string const & channel) {
  log->output("UNSUBSCRIBING TO " + channel);
  ws.send("{'event':'removeChannel', 'channel':'" + channel + "'}");
  channels[channel]->status = "unsubscribed";
}

string OKCoin::sign(string parameters) {
  // generate MD5
  unsigned char result[MD5_DIGEST_LENGTH];
  MD5((unsigned char*) parameters.c_str(), parameters.size(), result);
  ostringstream sout;
  sout << std::hex << std::setfill('0');
  for(long long c: result)
  {
    sout << std::setw(2) << (long long) c;
  }
  string signature = sout.str();
  // convert signature to upper case
  for (auto & c: signature) c = toupper(c);

  return signature;
}

string OKCoin::period_conversions(minutes period) {
  if (period == minutes(1))
    return "1min";
  if (period == minutes(15))
    return "15min";
  else
    return "";
}

minutes OKCoin::period_conversions(std::string okcoin_kline) {
  if (okcoin_kline == "1min")
    return minutes(1);
  if (okcoin_kline == "15min")
    return minutes(15);
  else
    return minutes(0);
}
