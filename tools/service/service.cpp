
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
            return true;
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

        void write_sys_log(const std::string &error_message, int log_level)
        {
            openlog("mail_distribution", log_level | LOG_PID, LOG_USER);
            syslog(LOG_NOTICE, "error %s", error_message.c_str());
            closelog();
        }

        unsigned char *char2uchar(const char *in)
        {
            unsigned long length = strlen(in);

            auto *out = new unsigned char[length + 1];

            for (auto i = 0; i < length; i++)
                out[i] = (unsigned char) in[i];

            out[length] = '\0';
            return out;
        }

        DataRange get_data_range(int row_count, int items_count, int order_number)
        {
            int begin = 0;
            int end = 0;
            int range = 0;
            if (row_count != 0 && items_count > 0) {
                range = row_count / items_count;
                begin = abs(order_number - 1) * range;
                end = begin + range;
                if (end > row_count) {
                    end = row_count;
                }
                ++begin;

            } else {
                throw std::runtime_error("invalid arguments");
            }
            return std::make_pair(begin, end);
        }

        std::ostream &operator<<(std::ostream &os, const StringListArray &array)
        {
            for (const auto& list: array) {
                os << list << std::endl;
            }
            return os;
        }
    }
}
