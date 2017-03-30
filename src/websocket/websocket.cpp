#include "websocket/websocket.h"

websocket::websocket(std::string uri) :
    thread_pool(4), status(Status::Connecting), server("N/A"), uri(uri) {
  setup();
}

websocket::~websocket() {
  teardown();
}

void websocket::setup() {
  endpoint.clear_access_channels(websocketpp::log::alevel::all);
  endpoint.clear_error_channels(websocketpp::log::elevel::all);

  endpoint.init_asio();
  endpoint.start_perpetual();

  thread = std::make_shared<websocketpp::lib::thread>(&wspp_client::run, &endpoint);
}

void websocket::teardown() {
  endpoint.stop_perpetual();

  if (status == Status::Open) {
    websocketpp::lib::error_code ec;
    endpoint.close(hdl, websocketpp::close::status::going_away, "", ec);
    if (ec)
      if (error_callback)
        error_callback(ec.message());
  }

  thread->join();
}

void websocket::connect() {
  websocketpp::lib::error_code ec;

  endpoint.set_open_handler(websocketpp::lib::bind(
      &websocket::on_open,
      this,
      &endpoint,
      websocketpp::lib::placeholders::_1
  ));
  endpoint.set_tls_init_handler(websocketpp::lib::bind(
      &websocket::on_tls_init,
      this,
      &endpoint,
      websocketpp::lib::placeholders::_1
  ));
  endpoint.set_message_handler(websocketpp::lib::bind(
      &websocket::on_message,
      this,
      websocketpp::lib::placeholders::_1,
      websocketpp::lib::placeholders::_2
  ));
  endpoint.set_fail_handler(websocketpp::lib::bind(
      &websocket::on_fail,
      this,
      &endpoint,
      websocketpp::lib::placeholders::_1
  ));
  endpoint.set_close_handler(websocketpp::lib::bind(
      &websocket::on_close,
      this,
      &endpoint,
      websocketpp::lib::placeholders::_1
  ));
  wspp_client::connection_ptr con = endpoint.get_connection(uri, ec);

  if (ec) {
    if (error_callback)
      error_callback(ec.message());
  }
  else {
    hdl = con->get_handle();
    endpoint.connect(con);
  }
}

void websocket::on_open(wspp_client *c, websocketpp::connection_hdl hdl) {
  status = Status::Open;
  wspp_client::connection_ptr con = c->get_con_from_hdl(hdl);
  server = con->get_response_header("Server");

  if (open_callback) {
    thread_pool.push([this](int thread_id) {
      open_callback();
    });
  }
}

wspp_context_ptr websocket::on_tls_init(wspp_client *, websocketpp::connection_hdl) {
  namespace asio = websocketpp::lib::asio;

  wspp_context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

  ctx->set_options(asio::ssl::context::default_workarounds |
                   asio::ssl::context::no_sslv2 |
                   asio::ssl::context::no_sslv3 |
                   asio::ssl::context::no_tlsv1 |
                   asio::ssl::context::single_dh_use);

  return ctx;
}

void websocket::on_message(websocketpp::connection_hdl, wspp_client::message_ptr msg) {
  if (message_callback) {
    std::string m = msg->get_payload();

    thread_pool.push([this, m = std::move(m)](int thread_id) {
      message_callback(m);
    });
  }
}

void websocket::on_close(wspp_client *c, websocketpp::connection_hdl hdl) {
  status = Status::Closed;

  wspp_client::connection_ptr con = c->get_con_from_hdl(hdl);
  std::ostringstream s;
  s << "close code: " << con->get_remote_close_code() << " ("
    << websocketpp::close::status::get_string(con->get_remote_close_code())
    << "), close reason: " << con->get_remote_close_reason();
  close_code = con->get_remote_close_code();
  error_reason = s.str();

  if (close_callback)
    close_callback();
}

void websocket::on_fail(wspp_client *c, websocketpp::connection_hdl hdl) {
  status = Status::Failed;
  wspp_client::connection_ptr con = c->get_con_from_hdl(hdl);
  server = con->get_response_header("Server");
  error_reason = con->get_ec().message();
  if (fail_callback)
    fail_callback();
}

void websocket::send(std::string message) {
  websocketpp::lib::error_code ec;

  endpoint.send(hdl, message, websocketpp::frame::opcode::text, ec);
  if (ec) {
    if (error_callback)
      error_callback(ec.message());
  }
}

void websocket::close(websocketpp::close::status::value code, std::string reason) {
  websocketpp::lib::error_code ec;

  endpoint.close(hdl, code, reason, ec);
  if (ec) {
    if (error_callback)
      error_callback(ec.message());
  }
}
