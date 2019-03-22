
#ifndef MAIL_DISTRIBUTION_SMTP_SERVER_HPP
#define MAIL_DISTRIBUTION_SMTP_SERVER_HPP


#include <vector>
#include <string.h>
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
#define TIME_IN_SEC        10        // how long client will wait for server response in non-blocking mode
#define BUFFER_SIZE 10240      // send_data and RecvData buffers sizes
#define MSG_SIZE_IN_MB 5        // the maximum size of the message with all attachments
#define COUNTER_VALUE    100        // how many times program will try to receive data

        const char BOUNDARY_TEXT[] = "__MESSAGE__ID__54yg6f6h6y456345";

        enum CSmptXPriority
        {
            XPRIORITY_HIGH = 2,
            XPRIORITY_NORMAL = 3,
            XPRIORITY_LOW = 4
        };

        class ECSmtp
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
                UNDEF_RECIPIENT_MAIL,
                COMMAND_MAIL_FROM = 300,
                COMMAND_EHLO,
                COMMAND_AUTH_LOGIN,
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
                LACK_OF_MEMORY,
                TIME_ERROR,
                RECVBUF_IS_EMPTY,
                SENDBUF_IS_EMPTY,
                OUT_OF_MSG_RANGE,
            };

            explicit ECSmtp(CSmtpError smtp_error)
                    : m_error_code(smtp_error)
            {

            }

            CSmtpError get_error_code() const
            {
                return m_error_code;
            }

            std::string get_error_message() const;

        private:
            CSmtpError m_error_code;
        };

        class CSmtp
        {
        public:
            CSmtp();

            virtual ~CSmtp();

            void add_recipient(const char *email, const char *name = nullptr);

            void add_BCC_recipient(const char *email, const char *name = nullptr);

            void add_CC_recipient(const char *email, const char *name = nullptr);

            void add_attachment(const char *path);

            void add_msg_line(const char *text);

            void delete_recipients();

            void delete_BCC_recipients();

            void delete_CC_recipients();

            void delete_attachments();

            void delete_message_lines();

            void delete_message_line(unsigned int line_number);

            void modify_message_line(unsigned int line_number, const char *text);

            unsigned int get_BCC_recipient_count() const;

            unsigned int get_CC_recipient_count() const;

            unsigned int get_recipient_count() const;

            const char *GetLocalHostIP() const;

            const char *get_local_hostname() const;

            const char *get_message_line_text(unsigned int line_number) const;

            unsigned int get_line_count() const;

            const char *get_reply_to() const;

            const char *get_mail_from() const;

            const char *get_sender_name() const;

            const char *get_subject() const;

            const char *get_xmailer() const;

            CSmptXPriority get_xpriority() const;

            void send_mail();

            void set_subject(const char *);

            void set_sender_name(const char *);

            void set_sender_mail(const char *);

            void set_reply_to(const char *);

            void set_xmailer(const char *);

            void set_login(const char *);

            void set_password(const char *);

            void set_xpriority(CSmptXPriority);

            void set_smtp_server(const char *server, const unsigned short port = 0);

        private:
            std::string m_local_hostname;
            std::string m_mail_from;
            std::string m_name_from;
            std::string m_subject;
            std::string m_mailer;
            std::string m_reply_to;
            std::string m_ip_address;
            std::string m_login;
            std::string m_password;
            std::string m_smtp_server_name;
            unsigned short m_smtp_server_port;
            CSmptXPriority m_xPriority;
            char *send_buffer;
            char *m_receive_buffer;

            SOCKET m_socket;

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

            void receive_data();

            void send_data();

            void format_header(char *);

            int smtp_xyz_digits();

            SOCKET connect_remote_server(const char *server, const unsigned short port = 0);

        };


    }
}

#endif //MAIL_DISTRIBUTION_SMTP_SERVER_HPP
