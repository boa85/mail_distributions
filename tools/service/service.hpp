
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

        enum CSmptXPriority
        {
            XPRIORITY_HIGH = 2,
            XPRIORITY_NORMAL = 3,
            XPRIORITY_LOW = 4
        };


        class SmtpException : public std::exception
        {

        public:
            enum CSmtpError
            {
                CSMTP_NO_ERROR = 0,
                WSA_STARTUP = 100, // WSAGetLastError()
                WSA_VER,
                WSA_SEND,
                WSA_RECV,
                WSA_CONNECT,
                WSA_GETHOSTBY_NAME_ADDR,
                WSA_INVALID_SOCKET,
                WSA_HOSTNAME,
                WSA_IOCTLSOCKET,
                WSA_SELECT,
                BAD_IPV4_ADDR,
                UNDEF_MSG_HEADER = 200,
                UNDEF_MAIL_FROM,
                UNDEF_SUBJECT,
                UNDEF_RECIPIENTS,
                UNDEF_LOGIN,
                UNDEF_PASSWORD,
                BAD_LOGIN_PASSWORD,
                BAD_DIGEST_RESPONSE,
                BAD_SERVER_NAME,
                UNDEF_RECIPIENT_MAIL,
                COMMAND_MAIL_FROM = 300,
                COMMAND_EHLO,
                COMMAND_AUTH_PLAIN,
                COMMAND_AUTH_LOGIN,
                COMMAND_AUTH_CRAMMD5,
                COMMAND_AUTH_DIGESTMD5,
                COMMAND_DIGESTMD5,
                COMMAND_DATA,
                COMMAND_QUIT,
                COMMAND_RCPT_TO,
                MSG_BODY_ERROR,
                CONNECTION_CLOSED = 400, // by server
                SERVER_NOT_READY, // remote server
                SERVER_NOT_RESPONDING,
                SELECT_TIMEOUT,
                FILE_NOT_EXIST,
                MSG_TOO_BIG,
                BAD_LOGIN_PASS,
                UNDEF_XYZ_RESPONSE,
                BAD_MEMORY_ALLOCC,
                TIME_ERROR,
                RECVBUF_IS_EMPTY,
                SENDBUF_IS_EMPTY,
                OUT_OF_MSG_RANGE,
                COMMAND_EHLO_STARTTLS,
                SSL_PROBLEM,
                COMMAND_DATABLOCK,
                STARTTLS_NOT_SUPPORTED,
                LOGIN_NOT_SUPPORTED
            };

            explicit SmtpException(CSmtpError smtp_error)
                    : m_error_code(smtp_error)
            {}

            CSmtpError get_error_code() const
            {
                return m_error_code;
            }

            std::string get_error_message();

        private:
            CSmtpError m_error_code;
        };


        enum SMTP_COMMAND
        {
            command_INIT,
            command_EHLO,
            command_AUTHPLAIN,
            command_AUTHLOGIN,
            command_AUTHCRAMMD5,
            command_AUTHDIGESTMD5,
            command_DIGESTMD5,
            command_USER,
            command_PASSWORD,
            command_MAILFROM,
            command_RCPTTO,
            command_DATA,
            command_DATABLOCK,
            command_DATAEND,
            command_QUIT,
            command_STARTTLS
        };

// TLS/SSL extension
        enum SMTP_SECURITY_TYPE
        {
            NO_SECURITY,
            USE_TLS,
            USE_SSL,
            DO_NOT_SET
        };


        typedef struct tagCommand_Entry
        {
            SMTP_COMMAND command;
            int send_timeout;     // 0 means no send is required
            int recv_timeout;     // 0 means no recv is required
            int valid_reply_code; // 0 means no recv is required, so no reply code
            SmtpException::CSmtpError error;
        } Command_Entry;

        static std::vector<Command_Entry> command_list =
                {
                        {command_INIT,          0,      5 * 60,  220, SmtpException::SERVER_NOT_RESPONDING},
                        {command_EHLO,          5 * 60, 5 * 60,  250, SmtpException::COMMAND_EHLO},
                        {command_AUTHPLAIN,     5 * 60, 5 * 60,  235, SmtpException::COMMAND_AUTH_PLAIN},
                        {command_AUTHLOGIN,     5 * 60, 5 * 60,  334, SmtpException::COMMAND_AUTH_LOGIN},
                        {command_AUTHCRAMMD5,   5 * 60, 5 * 60,  334, SmtpException::COMMAND_AUTH_CRAMMD5},
                        {command_AUTHDIGESTMD5, 5 * 60, 5 * 60,  334, SmtpException::COMMAND_AUTH_DIGESTMD5},
                        {command_DIGESTMD5,     5 * 60, 5 * 60,  335, SmtpException::COMMAND_DIGESTMD5},
                        {command_USER,          5 * 60, 5 * 60,  334, SmtpException::UNDEF_XYZ_RESPONSE},
                        {command_PASSWORD,      5 * 60, 5 * 60,  235, SmtpException::BAD_LOGIN_PASS},
                        {command_MAILFROM,      5 * 60, 5 * 60,  250, SmtpException::COMMAND_MAIL_FROM},
                        {command_RCPTTO,        5 * 60, 5 * 60,  250, SmtpException::COMMAND_RCPT_TO},
                        {command_DATA,          5 * 60, 2 * 60,  354, SmtpException::COMMAND_DATA},
                        {command_DATABLOCK,     3 *
                                                60,     0,       0,   SmtpException::COMMAND_DATABLOCK},    // Here the valid_reply_code is set to zero because there are no replies when sending data blocks
                        {command_DATAEND,       3 * 60, 10 * 60, 250, SmtpException::MSG_BODY_ERROR},
                        {command_QUIT,          5 * 60, 5 * 60,  221, SmtpException::COMMAND_QUIT},
                        {command_STARTTLS,      5 * 60, 5 * 60,  220, SmtpException::COMMAND_EHLO_STARTTLS}
                };

        Command_Entry *find_command_entry(SMTP_COMMAND command);

// A simple string match
        bool is_keyword_supported(const char *response, const char *keyword);

    }
}