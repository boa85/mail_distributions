
#pragma once

#include <vector>
#include <string>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
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

        using StringListArray = std::vector<StringList>;

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

            std::string m_username;
            std::string m_password;
            std::string m_database_name;
            std::string m_hostname;
            int m_port = 5432;

        };

        struct ServerConfig : Config
        {
            ServerConfig() : Config(), m_port(25)
            {
                m_type = CONFIG_TYPE::SERVER;
            }

            explicit ServerConfig(const KeyMap &keyMap) : Config(), m_port(25)
            {
                m_type = CONFIG_TYPE::SERVER;
                auto domain = keyMap.find("domain");
                if (domain != keyMap.end()) {
                    m_domain = domain->second;
                }
                auto port = keyMap.find("port");
                if (port != keyMap.end()) {
                    m_port = std::stoi(port->second);
                }
            }

            std::string m_domain;
            int m_port;

            bool is_valid() override
            {
                // todo: add correctness validation
                return m_port && !m_domain.empty();
            }
        };


        typedef std::shared_ptr<Config> ConfigPtr;

        typedef std::shared_ptr<DbConfig> DbConfigPtr;

        ConfigPtr read_config(const std::string &filename, CONFIG_TYPE config_type, SysErrorCode &error_code);

        void write_sys_log(const std::string &error_message);

        unsigned char *char2uchar(const char *in);



    }
}