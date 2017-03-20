#include "utilities/log.h"

Log::Log(std::string file_name, std::shared_ptr<Config> config) :
    config(config) {
  if (file_name == "") {
    log = &std::cout;
    output("LOGGING TO: std::cout");
  }
  else {
    log_file.open(file_name, std::ofstream::app);
    log = &log_file;
    output("LOGGING TO: " + file_name);
  }
};

Log::~Log() {
  output("CLOSING LOG STREAM");
  if (log_file.is_open())
    log_file.close();
}

void Log::output(std::string message, bool alert) {
  std::string s = ts_to_string(timestamp_now()) + ": " + message;
  if (alert)
    send_email(s);
  *log << s << std::endl;
};

void Log::send_email(std::string message) {
  if (!config->exists("alert_email_from") ||
      !config->exists("alert_email_name") ||
      !config->exists("alert_email_to") ||
      !config->exists("alert_email_pass")) {
    output("attempted to send alert email with no email config. message: " + message);
  }
  else {
    std::ofstream email_file("mail.txt", std::ofstream::out);
    email_file << "From: \"Bitcoin Algo\" <" << (*config)["alert_email_from"] << ">" << std::endl;
    email_file << "To: \"" << (*config)["alert_email_name"] << "\" <" << (*config)["alert_email_to"] << ">" << std::endl;
    email_file << "Subject: BITCOIN ALGO ALERT" << std::endl;
    email_file << std::endl << message;
    email_file.close();

    system(("curl --url \"smtps://smtp.gmail.com:465\" --ssl-reqd --mail-from \"" + (*config)["alert_email_to"] + "\" --mail-rcpt \"" + (*config)["alert_email_to"] + "\" -T mail.txt --user \"" + (*config)["alert_email_from"] + ":" + (*config)["alert_email_pass"] + "\"").c_str());

    remove("mail.txt");
  }
}
