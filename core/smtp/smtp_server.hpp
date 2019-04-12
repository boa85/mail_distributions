#pragma once

#include "../../tools/service/service.hpp"
#include "smtp_exception.hpp"
#include "smtp_common.hpp"
#include "smtp_statistic.hpp"


#include <vector>
#include <string.h>
#include <assert.h>

namespace md
{
    using namespace service;
    namespace smtp
    {
        class SmtpServer
        {
        public:


            std::string m_local_hostname;
            std::string m_mail_from;
            std::string m_name_from;
            std::string m_subject;
            std::string m_charset;
            std::string m_xmailer;
            std::string m_reply_to;
            bool m_is_read_receipt;
            std::string m_login;
            std::string m_password;
            std::string m_smtp_server_name;
            unsigned short m_smtp_server_port;
            bool m_is_authenticate;
            SMTP_XPRIORITY m_xpriority;
            char *m_send_buffer;
            char *m_receive_buffer;

            SOCKET m_socket;
            bool m_bConnected;

            struct Recipient
            {
                std::string m_name;
                std::string m_mail;
            };

            std::vector<Recipient> m_recipients;
            std::vector<Recipient> m_cc_recipients;
            std::vector<Recipient> m_bcc_recipients;
            std::vector<std::string> m_attachments;
            std::vector<std::string> m_message_body;

            SMTP_SECURITY_TYPE m_security_type;
            SSL_CTX *m_ctx;
            SSL *m_ssl;

            // statistics

            SmtpStatistic m_statistic;

        public:
            SmtpServer();

            virtual ~SmtpServer();

            void add_recipient(const char *email, const char *name = nullptr);

            void add_bcc_recipient(const char *email, const char *name = nullptr);

            void add_cc_recipient(const char *email, const char *name = nullptr);

            void add_attachment(const char *path);

            void add_message_line(const char *text);

            void clear_message();

            bool connect_remote_server(const char *szServer, unsigned short port = 0
                                       , SMTP_SECURITY_TYPE securityType = DO_NOT_SET
                                       , bool authenticate = true
                                       , const char *login = nullptr
                                       , const char *password = nullptr);

            void disconnect_remote_server();

            void delete_recipients();

            void delete_bcc_recipients();

            void delete_cc_recipients();

            void delete_attachments();

            void delete_message_lines();

            void delete_message_line(unsigned int line);

            void modify_message_line(unsigned int line, const char *text);

            const char *get_local_hostname();

            const char *get_message_line_text(unsigned int line) const;

            unsigned int get_gessage_line_count() const;

            bool send_mail();

            void set_charset(const char *charset);

            void set_subject(const char *subject);

            void set_sender_name(const char *name);

            void set_sender_mail(const char *email);

            void set_reply_to(const char *reply_to);

            void set_read_receipt(bool request_receipt = true);

            void set_xmailer(const char *xmailer);

            void set_login(const char *login);

            void set_password(const char *password);

            void set_xpriority(SMTP_XPRIORITY priority);

            void init_smtp_server(const char *server_name, unsigned short server_port = 0, bool authenticate = true);

            // TLS/SSL extension
            SMTP_SECURITY_TYPE get_security_type() const
            {
                return m_security_type;
            }

            void set_security_type(SMTP_SECURITY_TYPE type)
            {
                m_security_type = type;
            }

            bool m_bHTML;

            void init(const StringList &list, const std::string &smtp_hostname, unsigned int smtp_port);

            void inc_send_failed_count();

            void inc_send_success_count();

        private:

            void receive_data(Command_Entry *pEntry);

            void send_data(Command_Entry *pEntry);

            void format_header(char *);

            int SmtpXYZdigits();

            void say_hello();

            void say_quit();


            void receive_response(Command_Entry *pEntry);

            void init_open_ssl();

            void open_ssl_connect();

            void cleanup_open_ssl();

            void receive_data_SSL(SSL *ssl, Command_Entry *pEntry);

            void send_data_ssl(SSL *ssl, Command_Entry *pEntry);

            void start_tls();
        };

    }//namespace smtp
}//namespace md


