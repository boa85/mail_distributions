#pragma once

#include "../../tools/service/service.hpp"

#ifndef __CSMTP_H__
#define __CSMTP_H__


#include <vector>
#include <string.h>
#include <assert.h>

#ifdef __linux__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>

#include <openssl/ssl.h>

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

#ifndef HAVE__STRNICMP
#define HAVE__STRNICMP
#define _strnicmp strncasecmp
#endif

#define OutputDebugStringA(buf)

typedef unsigned short WORD;
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct hostent *LPHOSTENT;
typedef struct servent *LPSERVENT;
typedef struct in_addr *LPIN_ADDR;
typedef struct sockaddr *LPSOCKADDR;

#define LINUX
#else
#include <winsock2.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")

//Add "openssl-0.9.8l\inc32" to Additional Include Directories
#include "openssl\ssl.h"

#if _MSC_VER < 1400
#define snprintf _snprintf
#else
#define snprintf sprintf_s
#endif
#endif


#define TIME_IN_SEC        3*60    // how long client will wait for server response in non-blocking mode
#define BUFFER_SIZE        10240    // send_data and RecvData buffers sizes
#define MSG_SIZE_IN_MB    25        // the maximum size of the message with all attachments
#define COUNTER_VALUE    100        // how many times program will try to receive data

const char BOUNDARY_TEXT[] = "__MESSAGE__ID__54yg6f6h6y456345";

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
        LACK_OF_MEMORY,
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

    explicit SmtpException(CSmtpError error) : m_error_code(error)
    {}

    ~SmtpException() override = default;

    const char *what() const noexcept override;


    std::string get_error_message() const;

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

class SmtpServer
{
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
    CSmptXPriority m_xpriority;
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

    SMTP_SECURITY_TYPE m_type;
    SSL_CTX *m_ctx;
    SSL *m_ssl;

public:
    SmtpServer();

    virtual ~SmtpServer();

    void add_recipient(const char *email, const char *name = nullptr);

    void add_bcc_recipient(const char *email, const char *name = nullptr);

    void add_cc_recipient(const char *email, const char *name = nullptr);

    void add_attachment(const char *path);

    void add_message_line(const char *text);

    void clear_message();

    bool connect_remote_server(const char *szServer, unsigned short nPort_ = 0
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

    void send_mail();

    void set_charset(const char *charset);

    void set_subject(const char *subject);

    void set_sender_name(const char *name);

    void set_sender_mail(const char *email);

    void set_reply_to(const char *reply_to);

    void set_read_receipt(bool request_receipt = true);

    void set_xmailer(const char *xmailer);

    void set_login(const char *login);

    void set_password(const char *password);

    void set_xpriority(CSmptXPriority priority);

    void init_smtp_server(const char *server_name, unsigned short server_port = 0, bool authenticate = true);

    // TLS/SSL extension
    SMTP_SECURITY_TYPE get_security_type() const
    {
        return m_type;
    }

    void set_security_type(SMTP_SECURITY_TYPE type)
    {
        m_type = type;
    }

    bool m_bHTML;

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

Command_Entry *FindCommandEntry(SMTP_COMMAND command);

// A simple string match
bool IsKeywordSupported(const char *response, const char *keyword);

unsigned char *CharToUnsignedChar(const char *strIn);
#endif // __CSMTP_H__
