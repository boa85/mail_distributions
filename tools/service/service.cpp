
#include <regex>
#include <iostream>
#include <fstream>
#include "service.hpp"
#include <syslog.h>
#include <boost/asio/ip/address.hpp>

namespace md
{
    namespace service
    {
        bool is_valid_email(const std::string &email)
        {
            const std::regex pattern(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
            return std::regex_match(email, pattern);
        }

        bool is_valid_file(const std::string &filename, boost::system::error_code &errorCode)
        {
            return (fs::exists(filename, errorCode) && fs::is_regular(filename, errorCode));
        }


        bool is_valid_ip(const std::string &ip, SysErrorCode &error_code)
        {
            boost::asio::ip::address::from_string(ip, error_code);
            return !error_code;
        }

        std::shared_ptr<Config> read_config(
                const std::string &filename, CONFIG_TYPE config_type, SysErrorCode &error_code)
        {

            if (!is_valid_file(filename, error_code))
                return nullptr;

            KeyMap keyMap;
            std::ifstream file(filename);
            if (file.is_open()) {
                std::string line;
                while (getline(file, line)) {
                    line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());

                    if (line[0] == '#' || line.empty())
                        continue;

                    auto delimiterPos = line.find('=');
                    auto name = line.substr(0, delimiterPos);
                    auto value = line.substr(delimiterPos + 1);
                    keyMap.insert(std::make_pair(name, value));
                }

            } else {
                throw std::runtime_error("Couldn't open config file for reading");
            }
            switch (config_type) {
                case CONFIG_TYPE::NONE:
                    return nullptr;
                case CONFIG_TYPE::DATABASE:
                    return std::make_shared<DbConfig>(keyMap);
                case CONFIG_TYPE::SERVER:
                    return std::make_shared<ServerConfig>(keyMap);
            }
        }

        void write_sys_log(const std::string &error_message)
        {
            openlog("mail_distribution", LOG_PERROR | LOG_PID, LOG_USER);
            syslog(LOG_NOTICE, "error %s", error_message.c_str());
            closelog();
        }

    }
}
