#include <utility>

#include <utility>

#pragma once

#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string.h>

#include "../../tools/service/service.hpp"
#include <openssl/ssl.h>
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

typedef unsigned short WORD;
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct hostent *LPHOSTENT;
typedef struct servent *LPSERVENT;
typedef struct in_addr *LPIN_ADDR;
typedef struct sockaddr *LPSOCKADDR;

namespace md
{
    namespace smtp
    {
        using namespace service;
#define TIME_IN_SEC 10        // how long client will wait for server response in non-blocking mode
#define BUFFER_SIZE 10240      // send_data and RecvData buffers sizes
#define MSG_SIZE_IN_MB 5        // the maximum size of the message with all attachments
#define COUNTER_VALUE 100        // how many times program will try to receive data

        const char BOUNDARY_TEXT[] = "__MESSAGE__ID__54yg6f6h6y456345";


        class SmtpServer
        {

            std::string m_local_hostname;

            std::string m_mail_from;

            std::string m_name_from;

            std::string m_subject;

            std::string m_charset;

            std::string m_mailer;

            std::string m_reply_to;

            std::string m_ip_address;

            std::string m_login;

            std::string m_password;

            std::string m_smtp_server_name;

            unsigned short m_smtp_server_port;

            CSmptXPriority m_xPriority;

            char *m_send_buffer;

            char *m_receive_buffer;

            bool m_authenticate;

            SOCKET m_socket;

            struct Recipient
            {

                std::string m_name;
                std::string m_mail;

                Recipient() = default;

                Recipient(std::string name, std::string mail) : m_name(std::move(name)), m_mail(std::move(mail))
                {}
            };

            using Recipients = std::vector<Recipient>;

            Recipients m_recipients;

            Recipients m_cc_recipients;

            Recipients m_bcc_recipients;

            StringList m_attachments;

            StringList m_message_body;

            SMTP_SECURITY_TYPE m_security_type;

            SSL_CTX *m_ssl_ctx;

            SSL *m_ssl;

            bool m_bConnected;

            bool m_read_receipt;
        public:
////////////////////////////////////////////////////////////////////////////////////////
            SMTP_SECURITY_TYPE get_security_type() const;

            void set_security_type(SMTP_SECURITY_TYPE type);

            bool m_bHTML;
////////////////////////////////////////////////////////////////////////////////////////


            SmtpServer();

            virtual ~SmtpServer();

            void init(const StringList &list, const std::string &smtp_host = "", unsigned smtp_port = 25);

            void add_recipient(std::string email, std::string name);

            void add_BCC_recipient(std::string email, std::string name = "");

            void add_CC_recipient(std::string email, std::string name);

            void add_attachment(std::string &&path);

            void add_msg_line(std::string text);

            void set_charset(const std::string &charset);

            void delete_recipients();

            void delete_BCC_recipients();

            void delete_CC_recipients();

            void delete_attachments();

            void delete_message_lines();

            void delete_message_line(unsigned int line_number);

            void modify_message_line(unsigned int line_number, std::string text);

            void clear_message();

            unsigned long get_BCC_recipient_count() const;

            unsigned long get_CC_recipient_count() const;

            unsigned long get_recipient_count() const;


            const char *get_local_hostname();

            std::string get_message_line_text(unsigned int line_number) const;

            unsigned long get_line_count() const;

            std::string get_reply_to() const;

            std::string get_mail_from() const;

            std::string get_sender_name() const;

            std::string get_subject() const;

            std::string get_xmailer() const;

            CSmptXPriority get_xpriority() const;

            void send_mail();

            void set_subject(std::string);

            void set_sender_name(std::string name);

            void set_sender_mail(std::string email);

            void set_reply_to(std::string);

            void set_xmailer(std::string mailer);

            void set_login(std::string login);

            void set_password(std::string password);

            void set_xpriority(CSmptXPriority);

            void set_smtp_server(std::string server, unsigned int port = 25, bool authenticate = true);

        private:
            void receive_data(Command_Entry *pEntry);

            void send_data(Command_Entry *pEntry);

            void format_header(char *);

            int smtp_xyz_digits();

            void say_hello();

            void start_tls();

            void say_quit();

            SOCKET connect_remote_server(const char *server, unsigned short port = 0);

            bool ConnectRemoteServer(const char *szServer
                                     , const unsigned short nPort_ = 0
                                     , SMTP_SECURITY_TYPE securityType = DO_NOT_SET
                                     , bool authenticate = true
                                     , const char *login = NULL
                                     , const char *password = NULL);


            void receive_response(Command_Entry *pEntry);

            void init_openSSL();

            void openSSL_connect();

            void cleanup_openSSL();

            void receive_data_SSL(SSL *ssl, Command_Entry *pEntry);

            void send_data_SSL(SSL *ssl, Command_Entry *pEntry);


            void disconnect_remote_server();
        };


    }
}
