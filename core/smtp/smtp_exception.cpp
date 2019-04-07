#include "smtp_exception.hpp"
namespace md
{
    using namespace service;
    namespace smtp
    {
        std::string SmtpException::get_error_message() const
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
                    return "Undefined at least one reciepent";
                case SmtpException::UNDEF_RECIPIENT_MAIL:
                    return "Undefined recipent mail";
                case SmtpException::UNDEF_LOGIN:
                    return "Undefined user login";
                case SmtpException::UNDEF_PASSWORD:
                    return "Undefined user password";
                case SmtpException::BAD_LOGIN_PASSWORD:
                    return "Invalid user login or password";
                case SmtpException::BAD_DIGEST_RESPONSE:
                    return "Server returned a bad digest MD5 response";
                case SmtpException::BAD_SERVER_NAME:
                    return "Unable to determine server name for digest MD5 response";
                case SmtpException::COMMAND_MAIL_FROM:
                    return "Server returned error after sending MAIL FROM";
                case SmtpException::COMMAND_EHLO:
                    return "Server returned error after sending EHLO";
                case SmtpException::COMMAND_AUTH_PLAIN:
                    return "Server returned error after sending AUTH PLAIN";
                case SmtpException::COMMAND_AUTH_LOGIN:
                    return "Server returned error after sending AUTH LOGIN";
                case SmtpException::COMMAND_AUTH_CRAMMD5:
                    return "Server returned error after sending AUTH CRAM-MD5";
                case SmtpException::COMMAND_AUTH_DIGESTMD5:
                    return "Server returned error after sending AUTH DIGEST-MD5";
                case SmtpException::COMMAND_DIGESTMD5:
                    return "Server returned error after sending MD5 DIGEST";
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
                    return "Attachment file does not exist";
                case SmtpException::MSG_TOO_BIG:
                    return "Message is too big";
                case SmtpException::BAD_LOGIN_PASS:
                    return "Bad login or password";
                case SmtpException::UNDEF_XYZ_RESPONSE:
                    return "Undefined xyz SMTP response";
                case SmtpException::LACK_OF_MEMORY:
                    return "Lack of memory";
                case SmtpException::TIME_ERROR:
                    return "time() error";
                case SmtpException::RECVBUF_IS_EMPTY:
                    return "m_receive_buffer is empty";
                case SmtpException::SENDBUF_IS_EMPTY:
                    return "m_send_buffer is empty";
                case SmtpException::OUT_OF_MSG_RANGE:
                    return "Specified line number is out of message size";
                case SmtpException::COMMAND_EHLO_STARTTLS:
                    return "Server returned error after sending STARTTLS";
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

        const char *SmtpException::what() const noexcept
        {
            return exception::what();
        }

    }//namespace smtp
}//namespace md

