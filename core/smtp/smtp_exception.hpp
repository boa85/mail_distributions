#pragma once

#include <exception>
#include <string>
#include "../../tools/service/service.hpp"
namespace md
{
    using namespace service;
    namespace smtp
    {
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

    }//namespace smtp
}//namespace md

