
#pragma once

#include <vector>
#include <string>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <syslog.h>
#include <boost/signals2.hpp>
#include <boost/program_options.hpp>
#include <iostream>


namespace md
{
    namespace service
    {
        using KeyMap = std::map<std::string, std::string>;

        using SysErrorCode  = boost::system::error_code;

        namespace bs=boost::signals2;

        namespace po=boost::program_options;

        namespace fs=boost::filesystem;


        bool is_valid_file(const std::string &filename, SysErrorCode &errorCode);

        bool is_valid_email(const std::string &email);

        bool is_valid_ip(const std::string &ip, SysErrorCode &error_code);

        struct StringList : std::vector<std::string>
        {
            friend std::ostream &operator<<(std::ostream &os, const StringList &list)
            {
                for (const auto &string : list) {
                    os << string << " ";
                }
                return os;
            }
        };

        struct StringListArray : std::vector<StringList>
        {
            friend std::ostream &operator<<(std::ostream &os, const StringListArray &array);
        };

        enum class CONFIG_TYPE : int
        {
            NONE = -1,
            DATABASE = 1,
            SERVER = 2
        };

        struct Config
        {
            CONFIG_TYPE m_type;

            Config() : m_type(CONFIG_TYPE::NONE)
            {}

            virtual bool is_valid() = 0;
            virtual void print() = 0;
        };

        struct DbConfig : Config
        {
            DbConfig() : Config()
            {

            }

            explicit DbConfig(const KeyMap &keyMap) : Config()
            {
                m_type = CONFIG_TYPE::DATABASE;

                auto username = keyMap.find("username");
                if (username != keyMap.end()) {
                    m_username = username->second;
                }

                auto password = keyMap.find("password");
                if (password != keyMap.end()) {
                    m_password = password->second;
                }

                auto db_name = keyMap.find("db_name");
                if (db_name != keyMap.end()) {
                    m_database_name = db_name->second;
                }

                auto hostname = keyMap.find("hostname");
                if (hostname != keyMap.end()) {
                    m_hostname = (hostname->second);
                }
                auto port = keyMap.find("port");
                if (port != keyMap.end()) {
                    m_port = std::stoi(port->second);
                }
            }

            bool is_valid() override
            {
                SysErrorCode error_code;
                return !m_username.empty() && !m_password.empty() && !m_database_name.empty() &&
                       is_valid_ip(m_hostname, error_code);
            }

            void print() override
            {
                std::cout << "\nusername: " << m_username << "\npassword: " << m_password << "\ndatabase: "
                          << m_database_name << "\nhost: " << m_hostname << "\nport: " << m_port;
            }

            std::string m_username;
            std::string m_password;
            std::string m_database_name;
            std::string m_hostname;
            int m_port = 5432;

        };

        struct ServerConfig : Config
        {
        private:
            std::string m_domain;
            int m_port;
            int m_server_count;
            int m_order_number;
            int m_process_count;
        public:
            ServerConfig()
            : Config()
            , m_port(25)
            , m_server_count(0)
            , m_order_number(0)
            , m_process_count(0)
            {
                m_type = CONFIG_TYPE::SERVER;
            }

            explicit ServerConfig(const KeyMap &keyMap)
                    : Config()
                      , m_port(25)
                      , m_server_count(0)
                      , m_order_number(0)
                      , m_process_count(0)
            {
                m_type = CONFIG_TYPE::SERVER;
                auto it_domain = keyMap.find("domain");
                m_domain = it_domain != keyMap.end() ? it_domain->second : "";

                auto it_port = keyMap.find("port");
                m_port = it_port != keyMap.end() ? std::stoi(it_port->second) : -1;

                auto it_server_count = keyMap.find("server_count");
                m_server_count = it_server_count != keyMap.end() ? std::stoi(it_server_count->second) : -1;

                auto it_order_number = keyMap.find("order_number");
                m_order_number = it_order_number != keyMap.end() ? std::stoi(it_order_number->second) : -1;

                auto it_process_count = keyMap.find("process_count");
                m_process_count = it_process_count != keyMap.end() ? std::stoi(it_process_count->second) : -1;
            }

            bool is_valid() override
            {
                return
                        m_port != -1 &&
                        m_server_count != -1 &&
                        m_order_number != -1 &&
                        m_process_count != -1 &&
                        !m_domain.empty() &&
                        m_order_number < m_server_count;
            }

            void print() override
            {
                std::cout << "\ndomain: " << m_domain << "\nserver count: " << m_server_count << "\norder number: "
                          << m_order_number << "\nprocess: " << m_process_count << "\nport: " << m_port;
            }
            std::string get_domain() const
            {
                return m_domain;
            }

            int get_port() const
            {
                return m_port;
            }

            int get_server_count() const
            {
                return m_server_count;
            }

            int get_order_number() const
            {
                return m_order_number;
            }

            int get_process_count() const
            {
                return m_process_count;
            }
        };


        typedef std::shared_ptr<Config> ConfigPtr;

        typedef std::shared_ptr<DbConfig> DbConfigPtr;

        ConfigPtr read_config(const std::string &filename, CONFIG_TYPE config_type, SysErrorCode &error_code);

        void write_sys_log(const std::string &error_message, int log_level  = LOG_PERROR);

        unsigned char *char2uchar(const char *in);

        using DataRange = std::pair<int, int>;

        DataRange get_data_range(int row_count, int items_count, int order_number);
    }
}