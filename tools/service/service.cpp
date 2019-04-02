
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

        void write_sys_log(const std::string &error_message)
        {
            openlog("mail_distribution", LOG_PERROR | LOG_PID, LOG_USER);
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

        Command_Entry *find_command_entry(SMTP_COMMAND command)
        {
            for (auto &item : command_list) {
                if (item.command == command) {
                    return &item;
                }
            }
            return nullptr;
        }

        bool is_keyword_supported(const char *response, const char *keyword)
        {
            assert(response != nullptr && keyword != NULL);
            int res_len = strlen(response);
            int key_len = strlen(keyword);
            if (res_len < key_len)
                return false;

            for (int pos = 0; pos < res_len - key_len + 1; ++pos) {

                if (strncmp(keyword, response + pos, key_len) == 0) {

                    if (pos > 0 &&
                        (response[pos - 1] == '-' ||
                         response[pos - 1] == ' ' ||
                         response[pos - 1] == '=')) {
                        if (pos + key_len < res_len) {
                            if (response[pos + key_len] == ' ' ||
                                response[pos + key_len] == '=') {
                                return true;
                            } else if (pos + key_len + 1 < res_len) {
                                if (response[pos + key_len] == '\r' &&
                                    response[pos + key_len + 1] == '\n') {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            return false;
        }

        std::string SmtpException::get_error_message()
        {
            switch (m_error_code) {
                case SmtpException::CSMTP_NO_ERROR:
                    return "";
                case SmtpException::WSA_STARTUP:
                    return "Unable to initialise winsock2";
                case SmtpException::WSA_VER:
                    return "Wrong version of the winsock2";
                case SmtpException::WSA_SEND:
                    return "Function send() failed";
                case SmtpException::WSA_RECV:
                    return "Function recv() failed";
                case SmtpException::WSA_CONNECT:
                    return "Function connect failed";
                case SmtpException::WSA_GETHOSTBY_NAME_ADDR:
                    return "Unable to determine remote server";
                case SmtpException::WSA_INVALID_SOCKET:
                    return "Invalid winsock2 socket";
                case SmtpException::WSA_HOSTNAME:
                    return "Function hostname() failed";
                case SmtpException::WSA_IOCTLSOCKET:
                    return "Function ioctlsocket() failed";
                case SmtpException::BAD_IPV4_ADDR:
                    return "Improper IPv4 address";
                case SmtpException::UNDEF_MSG_HEADER:
                    return "Undefined message header";
                case SmtpException::UNDEF_MAIL_FROM:
                    return "Undefined mail sender";
                case SmtpException::UNDEF_SUBJECT:
                    return "Undefined message subject";
                case SmtpException::UNDEF_RECIPIENTS:
                    return "Undefined at least one recipient";
                case SmtpException::UNDEF_RECIPIENT_MAIL:
                    return "Undefined recipent mail";
                case SmtpException::UNDEF_LOGIN:
                    return "Undefined user login";
                case SmtpException::UNDEF_PASSWORD:
                    return "Undefined user password";
                case SmtpException::COMMAND_MAIL_FROM:
                    return "Server returned error after sending MAIL FROM";
                case SmtpException::COMMAND_EHLO:
                    return "Server returned error after sending EHLO";
                case SmtpException::COMMAND_AUTH_LOGIN:
                    return "Server returned error after sending AUTH LOGIN";
                case SmtpException::COMMAND_DATA:
                    return "Server returned error after sending DATA";
                case SmtpException::COMMAND_QUIT:
                    return "Server returned error after sending QUIT";
                case SmtpException::COMMAND_RCPT_TO:
                    return "Server returned error after sending RCPT TO";
                case SmtpException::MSG_BODY_ERROR:
                    return "Error in message body";
                case SmtpException::CONNECTION_CLOSED:
                    return "Server has closed the connection";
                case SmtpException::SERVER_NOT_READY:
                    return "Server is not ready";
                case SmtpException::SERVER_NOT_RESPONDING:
                    return "Server not responding";
                case SmtpException::FILE_NOT_EXIST:
                    return "File not exist";
                case SmtpException::MSG_TOO_BIG:
                    return "Message is too big";
                case SmtpException::BAD_LOGIN_PASS:
                    return "Bad login or password";
                case SmtpException::UNDEF_XYZ_RESPONSE:
                    return "Undefined xyz SMTP response";
                case SmtpException::BAD_MEMORY_ALLOCC:
                    return "Lack of memory";
                case SmtpException::TIME_ERROR:
                    return "time() error";
                case SmtpException::RECVBUF_IS_EMPTY:
                    return "m_receive_buffer is empty";
                case SmtpException::SENDBUF_IS_EMPTY:
                    return "m_send_buffer is empty";
                case SmtpException::OUT_OF_MSG_RANGE:
                    return "Specified line number is out of message size";

                case SmtpException::WSA_SELECT:
                    break;
                case SmtpException::SELECT_TIMEOUT:
                    break;
                case SmtpException::SSL_PROBLEM:
                    return "SSL problem";
                case SmtpException::COMMAND_DATABLOCK:
                    return "Failed to send data block";
                case SmtpException::STARTTLS_NOT_SUPPORTED:
                    return "The STARTTLS command is not supported by the server";
                case SmtpException::LOGIN_NOT_SUPPORTED:
                    return "AUTH LOGIN is not supported by the server";
                default:
                    return "Undefined error id";
            }
        }
    }
}
