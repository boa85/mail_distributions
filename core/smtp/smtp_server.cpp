#include <utility>
#include <zconf.h>
#include <vector>
#include "smtp_server.hpp"
#include "../../tools/service/base_64.hpp"
#include "md_5.hpp"
#include "openssl/err.h"
#include <cassert>
#include "../../tools/service/base_64.hpp"
#include "smtp_exception.hpp"

using namespace md::smtp;
namespace md
{
    using namespace service;
    namespace smtp
    {
////////////////////////////////////////////////////////////////////////////////
        SmtpServer::SmtpServer()
        {
            m_socket = INVALID_SOCKET;
            m_bConnected = false;
            m_xpriority = XPRIORITY_NORMAL;
            m_smtp_server_port = 0;
            m_is_authenticate = true;


            char hostname[255];
            if (gethostname((char *) &hostname, 255) == SOCKET_ERROR) throw SmtpException(SmtpException::WSA_HOSTNAME);
            m_local_hostname = hostname;

            if ((m_receive_buffer = new char[BUFFER_SIZE]) == nullptr)
                throw SmtpException(SmtpException::LACK_OF_MEMORY);

            if ((m_send_buffer = new char[BUFFER_SIZE]) == nullptr)
                throw SmtpException(SmtpException::LACK_OF_MEMORY);

            m_type = NO_SECURITY;
            m_ctx = nullptr;
            m_ssl = nullptr;
            m_bHTML = false;
            m_is_read_receipt = false;

            m_charset = "US-ASCII";
        }

////////////////////////////////////////////////////////////////////////////////
        SmtpServer::~SmtpServer()
        {
            if (m_bConnected) disconnect_remote_server();

            if (m_send_buffer) {
                delete[] m_send_buffer;
                m_send_buffer = nullptr;
            }
            if (m_receive_buffer) {
                delete[] m_receive_buffer;
                m_receive_buffer = nullptr;
            }

            cleanup_open_ssl();

#ifndef LINUX
            WSACleanup();
#endif
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::add_attachment(const char *path)
        {
            assert(path);
            m_attachments.insert(m_attachments.end(), path);
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::add_recipient(const char *email, const char *name)
        {
            if (!email)
                throw SmtpException(SmtpException::UNDEF_RECIPIENT_MAIL);

            Recipient recipient;
            recipient.m_mail = email;
            if (name != nullptr) recipient.m_name = name;
            else recipient.m_name.empty();

            m_recipients.insert(m_recipients.end(), recipient);
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::add_cc_recipient(const char *email, const char *name)
        {
            if (!email)
                throw SmtpException(SmtpException::UNDEF_RECIPIENT_MAIL);

            Recipient recipient;
            recipient.m_mail = email;
            if (name != nullptr) {
                recipient.m_name = name;
            } else {
                recipient.m_name.empty();
            }


            m_cc_recipients.insert(m_cc_recipients.end(), recipient);
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::add_bcc_recipient(const char *email, const char *name)
        {
            if (!email)
                throw SmtpException(SmtpException::UNDEF_RECIPIENT_MAIL);

            Recipient recipient;
            recipient.m_mail = email;
            if (name != nullptr) recipient.m_name = name;
            else recipient.m_name.empty();

            m_bcc_recipients.insert(m_bcc_recipients.end(), recipient);
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::add_message_line(const char *text)
        {
            m_message_body.insert(m_message_body.end(), text);
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::delete_message_line(unsigned int line)
        {
            if (line >= m_message_body.size())
                throw SmtpException(SmtpException::OUT_OF_MSG_RANGE);
            m_message_body.erase(m_message_body.begin() + line);
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::delete_recipients()
        {
            m_recipients.clear();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::delete_bcc_recipients()
        {
            m_bcc_recipients.clear();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::delete_cc_recipients()
        {
            m_cc_recipients.clear();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::delete_message_lines()
        {
            m_message_body.clear();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::delete_attachments()
        {
            m_attachments.clear();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::modify_message_line(unsigned int line, const char *text)
        {
            if (text) {
                if (line >= m_message_body.size())
                    throw SmtpException(SmtpException::OUT_OF_MSG_RANGE);
                m_message_body.at(line) = std::string(text);
            }
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::clear_message()
        {
            delete_recipients();
            delete_bcc_recipients();
            delete_cc_recipients();
            delete_attachments();
            delete_message_lines();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::send_mail()
        {
            unsigned int i, rcpt_count, res, FileId;
            char *FileBuf = nullptr;
            FILE *hFile = nullptr;
            unsigned long int FileSize, TotalSize, MsgPart;
            string FileName, EncodedFileName;
            string::size_type pos;

            // ***** CONNECTING TO SMTP SERVER *****

            // connecting to remote host if not already connected:
            if (m_socket == INVALID_SOCKET) {
                if (!connect_remote_server(m_smtp_server_name.c_str(), m_smtp_server_port, m_type, m_is_authenticate))
                    throw SmtpException(SmtpException::WSA_INVALID_SOCKET);
            }

            try {
                //Allocate memory
                if ((FileBuf = new char[55]) == nullptr)
                    throw SmtpException(SmtpException::LACK_OF_MEMORY);

                //Check that any attachments specified can be opened
                TotalSize = 0;
                for (FileId = 0; FileId < m_attachments.size(); FileId++) {
                    // opening the file:
                    hFile = fopen(m_attachments[FileId].c_str(), "rb");
                    if (hFile == nullptr)
                        throw SmtpException(SmtpException::FILE_NOT_EXIST);

                    // checking file size:
                    fseek(hFile, 0, SEEK_END);
                    FileSize = ftell(hFile);
                    TotalSize += FileSize;

                    // sending the file:
                    if (TotalSize / 1024 > MSG_SIZE_IN_MB * 1024)
                        throw SmtpException(SmtpException::MSG_TOO_BIG);

                    fclose(hFile);
                    hFile = nullptr;
                }

                // ***** SENDING E-MAIL *****

                // MAIL <SP> FROM:<reverse-path> <CRLF>
                if (!m_mail_from.size())
                    throw SmtpException(SmtpException::UNDEF_MAIL_FROM);
                Command_Entry *pEntry = find_command_entry(command_MAILFROM);
                snprintf(m_send_buffer, BUFFER_SIZE, "MAIL FROM:<%s>\r\n", m_mail_from.c_str());
                send_data(pEntry);
                receive_response(pEntry);

                // RCPT <SP> TO:<forward-path> <CRLF>
                if (!(rcpt_count = m_recipients.size()))
                    throw SmtpException(SmtpException::UNDEF_RECIPIENTS);
                pEntry = find_command_entry(command_RCPTTO);
                for (i = 0; i < m_recipients.size(); i++) {
                    snprintf(m_send_buffer, BUFFER_SIZE, "RCPT TO:<%s>\r\n", (m_recipients.at(i).m_mail).c_str());
                    send_data(pEntry);
                    receive_response(pEntry);
                }

                for (i = 0; i < m_cc_recipients.size(); i++) {
                    snprintf(m_send_buffer, BUFFER_SIZE, "RCPT TO:<%s>\r\n", (m_cc_recipients.at(i).m_mail).c_str());
                    send_data(pEntry);
                    receive_response(pEntry);
                }

                for (i = 0; i < m_bcc_recipients.size(); i++) {
                    snprintf(m_send_buffer, BUFFER_SIZE, "RCPT TO:<%s>\r\n", (m_bcc_recipients.at(i).m_mail).c_str());
                    send_data(pEntry);
                    receive_response(pEntry);
                }

                pEntry = find_command_entry(command_DATA);
                // DATA <CRLF>
                snprintf(m_send_buffer, BUFFER_SIZE, "DATA\r\n");
                send_data(pEntry);
                receive_response(pEntry);

                pEntry = find_command_entry(command_DATABLOCK);
                // send header(s)
                format_header(m_send_buffer);
                send_data(pEntry);

                // send text message
                if (get_gessage_line_count()) {
                    for (i = 0; i < get_gessage_line_count(); i++) {
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", get_message_line_text(i));
                        send_data(pEntry);
                    }
                } else {
                    snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", " ");
                    send_data(pEntry);
                }

                // next goes attachments (if they are)
                for (FileId = 0; FileId < m_attachments.size(); FileId++) {
#ifndef LINUX
                    pos = m_attachments[FileId].find_last_of("\\");
#else
                    pos = m_attachments[FileId].find_last_of("/");
#endif
                    if (pos == string::npos) FileName = m_attachments[FileId];
                    else FileName = m_attachments[FileId].substr(pos + 1);

                    //RFC 2047 - Use UTF-8 charset,base64 encode.
                    EncodedFileName = "=?UTF-8?B?";
                    EncodedFileName += base64_encode((unsigned char *) FileName.c_str(), FileName.size());
                    EncodedFileName += "?=";

                    snprintf(m_send_buffer, BUFFER_SIZE, "--%s\r\n", BOUNDARY_TEXT);
                    strcat(m_send_buffer, "Content-Type: application/x-msdownload; name=\"");
                    strcat(m_send_buffer, EncodedFileName.c_str());
                    strcat(m_send_buffer, "\"\r\n");
                    strcat(m_send_buffer, "Content-Transfer-Encoding: base64\r\n");
                    strcat(m_send_buffer, "Content-Disposition: attachment; filename=\"");
                    strcat(m_send_buffer, EncodedFileName.c_str());
                    strcat(m_send_buffer, "\"\r\n");
                    strcat(m_send_buffer, "\r\n");

                    send_data(pEntry);

                    // opening the file:
                    hFile = fopen(m_attachments[FileId].c_str(), "rb");
                    if (hFile == nullptr)
                        throw SmtpException(SmtpException::FILE_NOT_EXIST);

                    // get file size:
                    fseek(hFile, 0, SEEK_END);
                    FileSize = ftell(hFile);
                    fseek(hFile, 0, SEEK_SET);

                    MsgPart = 0;
                    for (i = 0; i < FileSize / 54 + 1; i++) {
                        res = fread(FileBuf, sizeof(char), 54, hFile);
                        MsgPart ? strcat(m_send_buffer, base64_encode(reinterpret_cast<const unsigned char *>(FileBuf), res).c_str())
                                : strcpy(m_send_buffer, base64_encode(reinterpret_cast<const unsigned char *>(FileBuf), res).c_str());
                        strcat(m_send_buffer, "\r\n");
                        MsgPart += res + 2;
                        if (MsgPart >= BUFFER_SIZE / 2) { // sending part of the message
                            MsgPart = 0;
                            send_data(pEntry); // FileBuf, FileName, fclose(hFile);
                        }
                    }
                    if (MsgPart) {
                        send_data(pEntry); // FileBuf, FileName, fclose(hFile);
                    }
                    fclose(hFile);
                    hFile = nullptr;
                }
                delete[] FileBuf;
                FileBuf = nullptr;

                // sending last message block (if there is one or more attachments)
                if (m_attachments.size()) {
                    snprintf(m_send_buffer, BUFFER_SIZE, "\r\n--%s--\r\n", BOUNDARY_TEXT);
                    send_data(pEntry);
                }

                pEntry = find_command_entry(command_DATAEND);
                // <CRLF> . <CRLF>
                snprintf(m_send_buffer, BUFFER_SIZE, "\r\n.\r\n");
                send_data(pEntry);
                receive_response(pEntry);
            }
            catch (const SmtpException &) {
                if (hFile) fclose(hFile);
                if (FileBuf) delete[] FileBuf;
                disconnect_remote_server();
                throw;
            }
        }

////////////////////////////////////////////////////////////////////////////////
        bool SmtpServer::connect_remote_server(const char *szServer, unsigned short nPort_/*=0*/
                                               , SMTP_SECURITY_TYPE securityType/*=DO_NOT_SET*/, bool authenticate/*=true*/
                                               , const char *login/*=nullptr*/, const char *password/*=nullptr*/)
        {
            unsigned short nPort = 0;
            LPSERVENT lpServEnt;
            SOCKADDR_IN sockAddr;
            unsigned long ul = 1;
            fd_set fdwrite, fdexcept;
            timeval timeout;
            int res = 0;

            try {
                timeout.tv_sec = TIME_IN_SEC;
                timeout.tv_usec = 0;

                m_socket = INVALID_SOCKET;

                if ((m_socket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
                    throw SmtpException(SmtpException::WSA_INVALID_SOCKET);

                if (nPort_ != 0)
                    nPort = htons(nPort_);
                else {
                    lpServEnt = getservbyname("mail", 0);
                    if (lpServEnt == nullptr)
                        nPort = htons(25);
                    else
                        nPort = lpServEnt->s_port;
                }

                sockAddr.sin_family = AF_INET;
                sockAddr.sin_port = nPort;
                if ((sockAddr.sin_addr.s_addr = inet_addr(szServer)) == INADDR_NONE) {
                    LPHOSTENT host;

                    host = gethostbyname(szServer);
                    if (host)
                        memcpy(&sockAddr.sin_addr, host->h_addr_list[0], host->h_length);
                    else {
#ifdef LINUX
                        close(m_socket);
#else
                        closesocket(m_socket);
#endif
                        throw SmtpException(SmtpException::WSA_GETHOSTBY_NAME_ADDR);
                    }
                }

                // start non-blocking mode for socket:
#ifdef LINUX
                if (ioctl(m_socket, FIONBIO, (unsigned long *) &ul) == SOCKET_ERROR)
#else
                    if(ioctlsocket(m_socket,FIONBIO, (unsigned long*)&ul) == SOCKET_ERROR)
#endif
                {
#ifdef LINUX
                    close(m_socket);
#else
                    closesocket(m_socket);
#endif
                    throw SmtpException(SmtpException::WSA_IOCTLSOCKET);
                }

                if (connect(m_socket, (LPSOCKADDR) &sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
#ifdef LINUX
                    if (errno != EINPROGRESS)
#else
                        if(WSAGetLastError() != WSAEWOULDBLOCK)
#endif
                    {
#ifdef LINUX
                        close(m_socket);
#else
                        closesocket(m_socket);
#endif
                        throw SmtpException(SmtpException::WSA_CONNECT);
                    }
                } else
                    return true;

                while (true) {
                    FD_ZERO(&fdwrite);
                    FD_ZERO(&fdexcept);

                    FD_SET(m_socket, &fdwrite);
                    FD_SET(m_socket, &fdexcept);

                    if ((res = select(m_socket + 1, nullptr, &fdwrite, &fdexcept, &timeout)) == SOCKET_ERROR) {
#ifdef LINUX
                        close(m_socket);
#else
                        closesocket(m_socket);
#endif
                        throw SmtpException(SmtpException::WSA_SELECT);
                    }

                    if (!res) {
#ifdef LINUX
                        close(m_socket);
#else
                        closesocket(m_socket);
#endif
                        throw SmtpException(SmtpException::SELECT_TIMEOUT);
                    }
                    if (res && FD_ISSET(m_socket, &fdwrite))
                        break;
                    if (res && FD_ISSET(m_socket, &fdexcept)) {
#ifdef LINUX
                        close(m_socket);
#else
                        closesocket(m_socket);
#endif
                        throw SmtpException(SmtpException::WSA_SELECT);
                    }
                } // while

                FD_CLR(m_socket, &fdwrite);
                FD_CLR(m_socket, &fdexcept);

                if (securityType != DO_NOT_SET) set_security_type(securityType);
                if (get_security_type() == USE_TLS || get_security_type() == USE_SSL) {
                    init_open_ssl();
                    if (get_security_type() == USE_SSL) {
                        open_ssl_connect();
                    }
                }

                Command_Entry *pEntry = find_command_entry(command_INIT);
                receive_response(pEntry);

                say_hello();

                if (get_security_type() == USE_TLS) {
                    start_tls();
                    say_hello();
                }

                if (authenticate && is_keyword_supported(m_receive_buffer, "AUTH") == true) {
                    if (login) set_login(login);
                    if (!m_login.size())
                        throw SmtpException(SmtpException::UNDEF_LOGIN);

                    if (password) set_password(password);
                    if (!m_password.size())
                        throw SmtpException(SmtpException::UNDEF_PASSWORD);

                    if (is_keyword_supported(m_receive_buffer, "LOGIN") == true) {
                        pEntry = find_command_entry(command_AUTHLOGIN);
                        snprintf(m_send_buffer, BUFFER_SIZE, "AUTH LOGIN\r\n");
                        send_data(pEntry);
                        receive_response(pEntry);

                        // send login:
                        std::string encoded_login = base64_encode(reinterpret_cast<const unsigned char *>(m_login.c_str()),
                                                                  m_login.size());
                        pEntry = find_command_entry(command_USER);
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", encoded_login.c_str());
                        send_data(pEntry);
                        receive_response(pEntry);

                        // send password:
                        std::string encoded_password = base64_encode(
                                reinterpret_cast<const unsigned char *>(m_password.c_str()), m_password.size());
                        pEntry = find_command_entry(command_PASSWORD);
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", encoded_password.c_str());
                        send_data(pEntry);
                        receive_response(pEntry);
                    } else if (is_keyword_supported(m_receive_buffer, "PLAIN") == true) {
                        pEntry = find_command_entry(command_AUTHPLAIN);
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s^%s^%s", m_login.c_str(), m_login.c_str(), m_password.c_str());
                        unsigned int length = strlen(m_send_buffer);
                        unsigned char *ustrLogin = char2uchar(m_send_buffer);
                        for (unsigned int i = 0; i < length; i++) {
                            if (ustrLogin[i] == 94) ustrLogin[i] = 0;
                        }
                        std::string encoded_login = base64_encode(ustrLogin, length);
                        delete[] ustrLogin;
                        snprintf(m_send_buffer, BUFFER_SIZE, "AUTH PLAIN %s\r\n", encoded_login.c_str());
                        send_data(pEntry);
                        receive_response(pEntry);
                    } else if (is_keyword_supported(m_receive_buffer, "CRAM-MD5") == true) {
                        pEntry = find_command_entry(command_AUTHCRAMMD5);
                        snprintf(m_send_buffer, BUFFER_SIZE, "AUTH CRAM-MD5\r\n");
                        send_data(pEntry);
                        receive_response(pEntry);

                        std::string encoded_challenge = m_receive_buffer;
                        encoded_challenge = encoded_challenge.substr(4);
                        std::string decoded_challenge = base64_decode(encoded_challenge);

                        /////////////////////////////////////////////////////////////////////
                        //test data from RFC 2195
                        //decoded_challenge = "<1896.697170952@postoffice.reston.mci.net>";
                        //m_login = "tim";
                        //m_password = "tanstaaftanstaaf";
                        //MD5 should produce b913a602c7eda7a495b4e6e7334d3890
                        //should encode as dGltIGI5MTNhNjAyYzdlZGE3YTQ5NWI0ZTZlNzMzNGQzODkw
                        /////////////////////////////////////////////////////////////////////

                        unsigned char *ustrChallenge = char2uchar(decoded_challenge.c_str());
                        unsigned char *ustrPassword = char2uchar(m_password.c_str());
                        if (!ustrChallenge || !ustrPassword)
                            throw SmtpException(SmtpException::BAD_LOGIN_PASSWORD);

                        // if ustrPassword is longer than 64 bytes reset it to ustrPassword=MD5(ustrPassword)
                        int passwordLength = m_password.size();
                        if (passwordLength > 64) {
                            MD5 md5password;
                            md5password.update(ustrPassword, passwordLength);
                            md5password.finalize();
                            ustrPassword = md5password.raw_digest();
                            passwordLength = 16;
                        }

                        //Storing ustrPassword in pads
                        unsigned char ipad[65], opad[65];
                        memset(ipad, 0, 64);
                        memset(opad, 0, 64);
                        memcpy(ipad, ustrPassword, passwordLength);
                        memcpy(opad, ustrPassword, passwordLength);

                        // XOR ustrPassword with ipad and opad values
                        for (int i = 0; i < 64; i++) {
                            ipad[i] ^= 0x36;
                            opad[i] ^= 0x5c;
                        }

                        //perform inner MD5
                        MD5 md5pass1;
                        md5pass1.update(ipad, 64);
                        md5pass1.update(ustrChallenge, decoded_challenge.size());
                        md5pass1.finalize();
                        unsigned char *ustrResult = md5pass1.raw_digest();

                        //perform outer MD5
                        MD5 md5pass2;
                        md5pass2.update(opad, 64);
                        md5pass2.update(ustrResult, 16);
                        md5pass2.finalize();
                        decoded_challenge = md5pass2.hex_digest();

                        delete[] ustrChallenge;
                        delete[] ustrPassword;
                        delete[] ustrResult;

                        decoded_challenge = m_login + " " + decoded_challenge;
                        encoded_challenge = base64_encode(reinterpret_cast<const unsigned char *>(decoded_challenge.c_str()),
                                                          decoded_challenge.size());

                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", encoded_challenge.c_str());
                        pEntry = find_command_entry(command_PASSWORD);
                        send_data(pEntry);
                        receive_response(pEntry);
                    } else if (is_keyword_supported(m_receive_buffer, "DIGEST-MD5") == true) {
                        pEntry = find_command_entry(command_DIGESTMD5);
                        snprintf(m_send_buffer, BUFFER_SIZE, "AUTH DIGEST-MD5\r\n");
                        send_data(pEntry);
                        receive_response(pEntry);

                        std::string encoded_challenge = m_receive_buffer;
                        encoded_challenge = encoded_challenge.substr(4);
                        std::string decoded_challenge = base64_decode(encoded_challenge);

                        /////////////////////////////////////////////////////////////////////
                        //Test data from RFC 2831
                        //To test jump into authenticate and read this line and the ones down to next test data section
                        //decoded_challenge = "realm=\"elwood.innosoft.com\",nonce=\"OA6MG9tEQGm2hh\",qop=\"auth\",algorithm=md5-sess,charset=utf-8";
                        /////////////////////////////////////////////////////////////////////

                        //Get the nonce (manditory)
                        int find = decoded_challenge.find("nonce");
                        if (find < 0)
                            throw SmtpException(SmtpException::BAD_DIGEST_RESPONSE);
                        std::string nonce = decoded_challenge.substr(find + 7);
                        find = nonce.find("\"");
                        if (find < 0)
                            throw SmtpException(SmtpException::BAD_DIGEST_RESPONSE);
                        nonce = nonce.substr(0, find);

                        //Get the realm (optional)
                        std::string realm;
                        find = decoded_challenge.find("realm");
                        if (find >= 0) {
                            realm = decoded_challenge.substr(find + 7);
                            find = realm.find("\"");
                            if (find < 0)
                                throw SmtpException(SmtpException::BAD_DIGEST_RESPONSE);
                            realm = realm.substr(0, find);
                        }

                        //Create a cnonce
                        char cnonce[17], nc[9];
                        snprintf(cnonce, 17, "%x", (unsigned int) time(nullptr));

                        //Set nonce count
                        snprintf(nc, 9, "%08d", 1);

                        //Set QOP
                        std::string qop = "auth";

                        //Get server address and set uri
                        //Skip this step during test
#ifdef __linux__
                        socklen_t len;
#else
                        int len;
#endif
                        struct sockaddr_storage addr;
                        len = sizeof addr;
                        if (!getpeername(m_socket, (struct sockaddr *) &addr, &len))
                            throw SmtpException(SmtpException::BAD_SERVER_NAME);

                        struct sockaddr_in *s = (struct sockaddr_in *) &addr;
                        std::string uri = inet_ntoa(s->sin_addr);
                        uri = "smtp/" + uri;

                        /////////////////////////////////////////////////////////////////////
                        //test data from RFC 2831
                        //m_login = "chris";
                        //m_password = "secret";
                        //snprintf(cnonce, 17, "OA6MHXh6VqTrRk");
                        //uri = "imap/elwood.innosoft.com";
                        //Should form the response:
                        //    charset=utf-8,username="chris",
                        //    realm="elwood.innosoft.com",nonce="OA6MG9tEQGm2hh",nc=00000001,
                        //    cnonce="OA6MHXh6VqTrRk",digest-uri="imap/elwood.innosoft.com",
                        //    response=d388dad90d4bbd760a152321f2143af7,qop=auth
                        //This encodes to:
                        //    Y2hhcnNldD11dGYtOCx1c2VybmFtZT0iY2hyaXMiLHJlYWxtPSJlbHdvb2
                        //    QuaW5ub3NvZnQuY29tIixub25jZT0iT0E2TUc5dEVRR20yaGgiLG5jPTAw
                        //    MDAwMDAxLGNub25jZT0iT0E2TUhYaDZWcVRyUmsiLGRpZ2VzdC11cmk9Im
                        //    ltYXAvZWx3b29kLmlubm9zb2Z0LmNvbSIscmVzcG9uc2U9ZDM4OGRhZDkw
                        //    ZDRiYmQ3NjBhMTUyMzIxZjIxNDNhZjcscW9wPWF1dGg=
                        /////////////////////////////////////////////////////////////////////

                        //Calculate digest response
                        unsigned char *ustrRealm = char2uchar(realm.c_str());
                        unsigned char *ustrUsername = char2uchar(m_login.c_str());
                        unsigned char *ustrPassword = char2uchar(m_password.c_str());
                        unsigned char *ustrNonce = char2uchar(nonce.c_str());
                        unsigned char *ustrCNonce = char2uchar(cnonce);
                        unsigned char *ustrUri = char2uchar(uri.c_str());
                        unsigned char *ustrNc = char2uchar(nc);
                        unsigned char *ustrQop = char2uchar(qop.c_str());
                        if (!ustrRealm || !ustrUsername || !ustrPassword || !ustrNonce || !ustrCNonce || !ustrUri || !ustrNc ||
                            !ustrQop)
                            throw SmtpException(SmtpException::BAD_LOGIN_PASSWORD);

                        MD5 md5a1a;
                        md5a1a.update(ustrUsername, m_login.size());
                        md5a1a.update((unsigned char *) ":", 1);
                        md5a1a.update(ustrRealm, realm.size());
                        md5a1a.update((unsigned char *) ":", 1);
                        md5a1a.update(ustrPassword, m_password.size());
                        md5a1a.finalize();
                        unsigned char *ua1 = md5a1a.raw_digest();

                        MD5 md5a1b;
                        md5a1b.update(ua1, 16);
                        md5a1b.update((unsigned char *) ":", 1);
                        md5a1b.update(ustrNonce, nonce.size());
                        md5a1b.update((unsigned char *) ":", 1);
                        md5a1b.update(ustrCNonce, strlen(cnonce));
                        //authzid could be added here
                        md5a1b.finalize();
                        char *a1 = md5a1b.hex_digest();

                        MD5 md5a2;
                        md5a2.update((unsigned char *) "AUTHENTICATE:", 13);
                        md5a2.update(ustrUri, uri.size());
                        //authint and authconf add an additional line here
                        md5a2.finalize();
                        char *a2 = md5a2.hex_digest();

                        delete[] ua1;
                        ua1 = char2uchar(a1);
                        unsigned char *ua2 = char2uchar(a2);

                        //compute KD
                        MD5 md5;
                        md5.update(ua1, 32);
                        md5.update((unsigned char *) ":", 1);
                        md5.update(ustrNonce, nonce.size());
                        md5.update((unsigned char *) ":", 1);
                        md5.update(ustrNc, strlen(nc));
                        md5.update((unsigned char *) ":", 1);
                        md5.update(ustrCNonce, strlen(cnonce));
                        md5.update((unsigned char *) ":", 1);
                        md5.update(ustrQop, qop.size());
                        md5.update((unsigned char *) ":", 1);
                        md5.update(ua2, 32);
                        md5.finalize();
                        decoded_challenge = md5.hex_digest();

                        delete[] ustrRealm;
                        delete[] ustrUsername;
                        delete[] ustrPassword;
                        delete[] ustrNonce;
                        delete[] ustrCNonce;
                        delete[] ustrUri;
                        delete[] ustrNc;
                        delete[] ustrQop;
                        delete[] ua1;
                        delete[] ua2;
                        delete[] a1;
                        delete[] a2;

                        //send the response
                        if (strstr(m_receive_buffer, "charset") >= 0)
                            snprintf(m_send_buffer, BUFFER_SIZE, "charset=utf-8,username=\"%s\"", m_login.c_str());
                        else snprintf(m_send_buffer, BUFFER_SIZE, "username=\"%s\"", m_login.c_str());
                        if (!realm.empty()) {
                            snprintf(m_receive_buffer, BUFFER_SIZE, ",realm=\"%s\"", realm.c_str());
                            strcat(m_send_buffer, m_receive_buffer);
                        }
                        snprintf(m_receive_buffer, BUFFER_SIZE, ",nonce=\"%s\"", nonce.c_str());
                        strcat(m_send_buffer, m_receive_buffer);
                        snprintf(m_receive_buffer, BUFFER_SIZE, ",nc=%s", nc);
                        strcat(m_send_buffer, m_receive_buffer);
                        snprintf(m_receive_buffer, BUFFER_SIZE, ",cnonce=\"%s\"", cnonce);
                        strcat(m_send_buffer, m_receive_buffer);
                        snprintf(m_receive_buffer, BUFFER_SIZE, ",digest-uri=\"%s\"", uri.c_str());
                        strcat(m_send_buffer, m_receive_buffer);
                        snprintf(m_receive_buffer, BUFFER_SIZE, ",response=%s", decoded_challenge.c_str());
                        strcat(m_send_buffer, m_receive_buffer);
                        snprintf(m_receive_buffer, BUFFER_SIZE, ",qop=%s", qop.c_str());
                        strcat(m_send_buffer, m_receive_buffer);
                        unsigned char *ustrDigest = char2uchar(m_send_buffer);
                        encoded_challenge = base64_encode(ustrDigest, strlen(m_send_buffer));
                        delete[] ustrDigest;
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", encoded_challenge.c_str());
                        pEntry = find_command_entry(command_DIGESTMD5);
                        send_data(pEntry);
                        receive_response(pEntry);

                        //Send completion carraige return
                        snprintf(m_send_buffer, BUFFER_SIZE, "\r\n");
                        pEntry = find_command_entry(command_PASSWORD);
                        send_data(pEntry);
                        receive_response(pEntry);
                    } else throw SmtpException(SmtpException::LOGIN_NOT_SUPPORTED);
                }
            }
            catch (const SmtpException &) {
                if (m_receive_buffer[0] == '5' && m_receive_buffer[1] == '3' && m_receive_buffer[2] == '0')
                    m_bConnected = false;
                disconnect_remote_server();
                throw;
                return false;
            }

            return true;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::disconnect_remote_server()
        {
            if (m_bConnected) say_quit();
            if (m_socket) {
#ifdef LINUX
                close(m_socket);
#else
                closesocket(m_socket);
#endif
            }
            m_socket = INVALID_SOCKET;
        }

////////////////////////////////////////////////////////////////////////////////
        int SmtpServer::SmtpXYZdigits()
        {
            assert(m_receive_buffer);
            if (m_receive_buffer == nullptr)
                return 0;
            return (m_receive_buffer[0] - '0') * 100 + (m_receive_buffer[1] - '0') * 10 + m_receive_buffer[2] - '0';
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::format_header(char *header)
        {
            char month[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            size_t i;
            std::string to;
            std::string cc;
            std::string bcc;
            time_t rawtime;
            struct tm *timeinfo;

            // date/time check
            if (time(&rawtime) > 0)
                timeinfo = localtime(&rawtime);
            else
                throw SmtpException(SmtpException::TIME_ERROR);

            // check for at least one recipient
            if (m_recipients.size()) {
                for (i = 0; i < m_recipients.size(); i++) {
                    if (i > 0)
                        to.append(",");
                    to += m_recipients[i].m_name;
                    to.append("<");
                    to += m_recipients[i].m_mail;
                    to.append(">");
                }
            } else
                throw SmtpException(SmtpException::UNDEF_RECIPIENTS);

            if (m_cc_recipients.size()) {
                for (i = 0; i < m_cc_recipients.size(); i++) {
                    if (i > 0)
                        cc.append(",");
                    cc += m_cc_recipients[i].m_name;
                    cc.append("<");
                    cc += m_cc_recipients[i].m_mail;
                    cc.append(">");
                }
            }

            // Date: <SP> <dd> <SP> <mon> <SP> <yy> <SP> <hh> ":" <mm> ":" <ss> <SP> <zone> <CRLF>
            snprintf(header, BUFFER_SIZE, "Date: %d %s %d %d:%d:%d\r\n", timeinfo->tm_mday,
                     month[timeinfo->tm_mon], timeinfo->tm_year + 1900, timeinfo->tm_hour,
                     timeinfo->tm_min, timeinfo->tm_sec);

            // From: <SP> <sender>  <SP> "<" <sender-email> ">" <CRLF>
            if (!m_mail_from.size()) throw SmtpException(SmtpException::UNDEF_MAIL_FROM);

            strcat(header, "From: ");
            if (m_name_from.size()) strcat(header, m_name_from.c_str());

            strcat(header, " <");
            strcat(header, m_mail_from.c_str());
            strcat(header, ">\r\n");

            // X-Mailer: <SP> <xmailer-app> <CRLF>
            if (m_xmailer.size()) {
                strcat(header, "X-Mailer: ");
                strcat(header, m_xmailer.c_str());
                strcat(header, "\r\n");
            }

            // Reply-To: <SP> <reverse-path> <CRLF>
            if (m_reply_to.size()) {
                strcat(header, "Reply-To: ");
                strcat(header, m_reply_to.c_str());
                strcat(header, "\r\n");
            }

            // Disposition-Notification-To: <SP> <reverse-path or sender-email> <CRLF>
            if (m_is_read_receipt) {
                strcat(header, "Disposition-Notification-To: ");
                if (m_reply_to.size()) strcat(header, m_reply_to.c_str());
                else strcat(header, m_name_from.c_str());
                strcat(header, "\r\n");
            }

            // X-Priority: <SP> <number> <CRLF>
            switch (m_xpriority) {
                case XPRIORITY_HIGH:
                    strcat(header, "X-Priority: 2 (High)\r\n");
                    break;
                case XPRIORITY_NORMAL:
                    strcat(header, "X-Priority: 3 (Normal)\r\n");
                    break;
                case XPRIORITY_LOW:
                    strcat(header, "X-Priority: 4 (Low)\r\n");
                    break;
                default:
                    strcat(header, "X-Priority: 3 (Normal)\r\n");
            }

            // To: <SP> <remote-user-mail> <CRLF>
            strcat(header, "To: ");
            strcat(header, to.c_str());
            strcat(header, "\r\n");

            // Cc: <SP> <remote-user-mail> <CRLF>
            if (m_cc_recipients.size()) {
                strcat(header, "Cc: ");
                strcat(header, cc.c_str());
                strcat(header, "\r\n");
            }

            if (m_bcc_recipients.size()) {
                strcat(header, "Bcc: ");
                strcat(header, bcc.c_str());
                strcat(header, "\r\n");
            }

            // Subject: <SP> <subject-text> <CRLF>
            if (!m_subject.size())
                strcat(header, "Subject:  ");
            else {
                strcat(header, "Subject: ");
                strcat(header, m_subject.c_str());
            }
            strcat(header, "\r\n");

            // MIME-Version: <SP> 1.0 <CRLF>
            strcat(header, "MIME-Version: 1.0\r\n");
            if (!m_attachments.size()) { // no attachments
                if (m_bHTML) strcat(header, "Content-Type: text/html; charset=\"");
                else strcat(header, "Content-type: text/plain; charset=\"");
                strcat(header, m_charset.c_str());
                strcat(header, "\"\r\n");
                strcat(header, "Content-Transfer-Encoding: 7bit\r\n");
                strcat(m_send_buffer, "\r\n");
            } else { // there is one or more attachments
                strcat(header, "Content-Type: multipart/mixed; boundary=\"");
                strcat(header, BOUNDARY_TEXT);
                strcat(header, "\"\r\n");
                strcat(header, "\r\n");
                // first goes text message
                strcat(m_send_buffer, "--");
                strcat(m_send_buffer, BOUNDARY_TEXT);
                strcat(m_send_buffer, "\r\n");
                if (m_bHTML) strcat(m_send_buffer, "Content-type: text/html; charset=");
                else strcat(m_send_buffer, "Content-type: text/plain; charset=");
                strcat(header, m_charset.c_str());
                strcat(header, "\r\n");
                strcat(m_send_buffer, "Content-Transfer-Encoding: 7bit\r\n");
                strcat(m_send_buffer, "\r\n");
            }

            // done
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::receive_data(Command_Entry *pEntry)
        {
            if (m_ssl != nullptr) {
                receive_data_SSL(m_ssl, pEntry);
                return;
            }
            int res = 0;
            fd_set fdread;
            timeval time;

            time.tv_sec = pEntry->recv_timeout;
            time.tv_usec = 0;

            assert(m_receive_buffer);

            if (m_receive_buffer == nullptr)
                throw SmtpException(SmtpException::RECVBUF_IS_EMPTY);

            FD_ZERO(&fdread);

            FD_SET(m_socket, &fdread);

            if ((res = select(m_socket + 1, &fdread, nullptr, nullptr, &time)) == SOCKET_ERROR) {
                FD_CLR(m_socket, &fdread);
                throw SmtpException(SmtpException::WSA_SELECT);
            }

            if (!res) {
                //timeout
                FD_CLR(m_socket, &fdread);
                throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
            }

            if (FD_ISSET(m_socket, &fdread)) {
                res = recv(m_socket, m_receive_buffer, BUFFER_SIZE, 0);
                if (res == SOCKET_ERROR) {
                    FD_CLR(m_socket, &fdread);
                    throw SmtpException(SmtpException::WSA_RECV);
                }
            }

            FD_CLR(m_socket, &fdread);
            m_receive_buffer[res] = 0;
            if (res == 0) {
                throw SmtpException(SmtpException::CONNECTION_CLOSED);
            }
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::send_data(Command_Entry *pEntry)
        {
            if (m_ssl != nullptr) {
                send_data_ssl(m_ssl, pEntry);
                return;
            }
            int idx = 0, res, nLeft = strlen(m_send_buffer);
            fd_set fdwrite;
            timeval time;

            time.tv_sec = pEntry->send_timeout;
            time.tv_usec = 0;

            assert(m_send_buffer);

            if (m_send_buffer == nullptr)
                throw SmtpException(SmtpException::SENDBUF_IS_EMPTY);

            while (nLeft > 0) {
                FD_ZERO(&fdwrite);

                FD_SET(m_socket, &fdwrite);

                if ((res = select(m_socket + 1, nullptr, &fdwrite, nullptr, &time)) == SOCKET_ERROR) {
                    FD_CLR(m_socket, &fdwrite);
                    throw SmtpException(SmtpException::WSA_SELECT);
                }

                if (!res) {
                    //timeout
                    FD_CLR(m_socket, &fdwrite);
                    throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
                }

                if (res && FD_ISSET(m_socket, &fdwrite)) {
                    res = send(m_socket, &m_send_buffer[idx], nLeft, 0);
                    if (res == SOCKET_ERROR || res == 0) {
                        FD_CLR(m_socket, &fdwrite);
                        throw SmtpException(SmtpException::WSA_SEND);
                    }
                    nLeft -= res;
                    idx += res;
                }
            }

            FD_CLR(m_socket, &fdwrite);
        }

////////////////////////////////////////////////////////////////////////////////
        const char *SmtpServer::get_local_hostname()
        {
            return m_local_hostname.c_str();
        }

        const char *SmtpServer::get_message_line_text(unsigned int line) const
        {
            if (line >= m_message_body.size())
                throw SmtpException(SmtpException::OUT_OF_MSG_RANGE);
            return m_message_body.at(line).c_str();
        }

        unsigned int SmtpServer::get_gessage_line_count() const
        {
            return m_message_body.size();
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_charset(const char *charset)
        {
            m_charset = charset;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_xpriority(SMTP_XPRIORITY priority)
        {
            m_xpriority = priority;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_reply_to(const char *reply_to)
        {
            m_reply_to = reply_to;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_read_receipt(bool request_receipt)
        {
            m_is_read_receipt = request_receipt;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_sender_mail(const char *email)
        {
            m_mail_from = email;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_sender_name(const char *name)
        {
            m_name_from = name;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_subject(const char *subject)
        {
            m_subject = subject;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_xmailer(const char *xmailer)
        {
            m_xmailer = xmailer;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_login(const char *login)
        {
            m_login = login;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::set_password(const char *password)
        {
            m_password = password;
        }

////////////////////////////////////////////////////////////////////////////////
        void SmtpServer::init_smtp_server(const char *server_name, const unsigned short server_port, bool authenticate)
        {
            m_smtp_server_port = server_port;
            m_smtp_server_name = server_name;
            m_is_authenticate = authenticate;
        }

////////////////////////////////////////////////////////////////////////////////

        void SmtpServer::say_hello()
        {
            Command_Entry *pEntry = find_command_entry(command_EHLO);
            snprintf(m_send_buffer, BUFFER_SIZE, "EHLO %s\r\n", get_local_hostname() != nullptr ? m_local_hostname.c_str() : "domain");
            send_data(pEntry);
            receive_response(pEntry);
            m_bConnected = true;
        }

        void SmtpServer::say_quit()
        {
            // ***** CLOSING CONNECTION *****

            Command_Entry *pEntry = find_command_entry(command_QUIT);
            // QUIT <CRLF>
            snprintf(m_send_buffer, BUFFER_SIZE, "QUIT\r\n");
            m_bConnected = false;
            send_data(pEntry);
            receive_response(pEntry);
        }

        void SmtpServer::start_tls()
        {
            if (!is_keyword_supported(m_receive_buffer, "STARTTLS")) {
                throw SmtpException(SmtpException::STARTTLS_NOT_SUPPORTED);
            }
            Command_Entry *pEntry = find_command_entry(command_STARTTLS);
            snprintf(m_send_buffer, BUFFER_SIZE, "STARTTLS\r\n");
            send_data(pEntry);
            receive_response(pEntry);

            open_ssl_connect();
        }

        void SmtpServer::receive_data_SSL(SSL *ssl, Command_Entry *pEntry)
        {
            int res = 0;
            int offset = 0;
            fd_set fdread;
            fd_set fdwrite;
            timeval time;

            int read_blocked_on_write = 0;

            time.tv_sec = pEntry->recv_timeout;
            time.tv_usec = 0;

            assert(m_receive_buffer);

            if (m_receive_buffer == nullptr)
                throw SmtpException(SmtpException::RECVBUF_IS_EMPTY);

            bool bFinish = false;

            while (!bFinish) {
                FD_ZERO(&fdread);
                FD_ZERO(&fdwrite);

                FD_SET(m_socket, &fdread);

                if (read_blocked_on_write) {
                    FD_SET(m_socket, &fdwrite);
                }

                if ((res = select(m_socket + 1, &fdread, &fdwrite, nullptr, &time)) == SOCKET_ERROR) {
                    FD_ZERO(&fdread);
                    FD_ZERO(&fdwrite);
                    throw SmtpException(SmtpException::WSA_SELECT);
                }

                if (!res) {
                    //timeout
                    FD_ZERO(&fdread);
                    FD_ZERO(&fdwrite);
                    throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
                }

                if (FD_ISSET(m_socket, &fdread) || (read_blocked_on_write && FD_ISSET(m_socket, &fdwrite))) {
                    while (1) {
                        read_blocked_on_write = 0;

                        const int buff_len = 1024;
                        char buff[buff_len];

                        res = SSL_read(ssl, buff, buff_len);

                        int ssl_err = SSL_get_error(ssl, res);
                        if (ssl_err == SSL_ERROR_NONE) {
                            if (offset + res > BUFFER_SIZE - 1) {
                                FD_ZERO(&fdread);
                                FD_ZERO(&fdwrite);
                                throw SmtpException(SmtpException::LACK_OF_MEMORY);
                            }
                            memcpy(m_receive_buffer + offset, buff, res);
                            offset += res;
                            if (SSL_pending(ssl)) {
                                continue;
                            } else {
                                bFinish = true;
                                break;
                            }
                        } else if (ssl_err == SSL_ERROR_ZERO_RETURN) {
                            bFinish = true;
                            break;
                        } else if (ssl_err == SSL_ERROR_WANT_READ) {
                            break;
                        } else if (ssl_err == SSL_ERROR_WANT_WRITE) {
                            /* We get a WANT_WRITE if we're
                            trying to rehandshake and we block on
                            a write during that rehandshake.

                            We need to wait on the socket to be
                            writeable but reinitiate the read
                            when it is */
                            read_blocked_on_write = 1;
                            break;
                        } else {
                            FD_ZERO(&fdread);
                            FD_ZERO(&fdwrite);
                            throw SmtpException(SmtpException::SSL_PROBLEM);
                        }
                    }
                }
            }

            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            m_receive_buffer[offset] = 0;
            if (offset == 0) {
                throw SmtpException(SmtpException::CONNECTION_CLOSED);
            }
        }

        void SmtpServer::receive_response(Command_Entry *pEntry)
        {
            std::string line;
            int reply_code = 0;
            bool bFinish = false;
            while (!bFinish) {
                receive_data(pEntry);
                line.append(m_receive_buffer);
                size_t len = line.length();
                size_t begin = 0;
                size_t offset = 0;

                while (true) // loop for all lines
                {
                    while (offset + 1 < len) {
                        if (line[offset] == '\r' && line[offset + 1] == '\n')
                            break;
                        ++offset;
                    }
                    if (offset + 1 < len) // we found a line
                    {
                        // see if this is the last line
                        // the last line must match the pattern: XYZ<SP>*<CRLF> or XYZ<CRLF> where XYZ is a string of 3 digits
                        offset += 2; // skip <CRLF>
                        if (offset - begin >= 5) {
                            if (isdigit(line[begin]) && isdigit(line[begin + 1]) && isdigit(line[begin + 2])) {
                                // this is the last line
                                if (offset - begin == 5 || line[begin + 3] == ' ') {
                                    reply_code =
                                            (line[begin] - '0') * 100 + (line[begin + 1] - '0') * 10 + line[begin + 2] - '0';
                                    bFinish = true;
                                    break;
                                }
                            }
                        }
                        begin = offset;    // try to find next line
                    } else // we haven't received the last line, so we need to receive more data
                    {
                        break;
                    }
                }
            }
            snprintf(m_receive_buffer, BUFFER_SIZE, line.c_str());
            if (reply_code != pEntry->valid_reply_code) {
                throw SmtpException(pEntry->error);
            }
        }

        void SmtpServer::send_data_ssl(SSL *ssl, Command_Entry *pEntry)
        {
            int offset = 0, res, nLeft = strlen(m_send_buffer);
            fd_set fdwrite;
            fd_set fdread;
            timeval time;

            int write_blocked_on_read = 0;

            time.tv_sec = pEntry->send_timeout;
            time.tv_usec = 0;

            assert(m_send_buffer);

            if (m_send_buffer == nullptr)
                throw SmtpException(SmtpException::SENDBUF_IS_EMPTY);

            while (nLeft > 0) {
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdread);

                FD_SET(m_socket, &fdwrite);

                if (write_blocked_on_read) {
                    FD_SET(m_socket, &fdread);
                }

                if ((res = select(m_socket + 1, &fdread, &fdwrite, nullptr, &time)) == SOCKET_ERROR) {
                    FD_ZERO(&fdwrite);
                    FD_ZERO(&fdread);
                    throw SmtpException(SmtpException::WSA_SELECT);
                }

                if (!res) {
                    //timeout
                    FD_ZERO(&fdwrite);
                    FD_ZERO(&fdread);
                    throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
                }

                if (FD_ISSET(m_socket, &fdwrite) || (write_blocked_on_read && FD_ISSET(m_socket, &fdread))) {
                    write_blocked_on_read = 0;

                    /* Try to write */
                    res = SSL_write(ssl, m_send_buffer + offset, nLeft);

                    switch (SSL_get_error(ssl, res)) {
                        /* We wrote something*/
                        case SSL_ERROR_NONE:
                            nLeft -= res;
                            offset += res;
                            break;

                            /* We would have blocked */
                        case SSL_ERROR_WANT_WRITE:
                            break;

                            /* We get a WANT_READ if we're
                               trying to rehandshake and we block on
                               write during the current connection.

                               We need to wait on the socket to be readable
                               but reinitiate our write when it is */
                        case SSL_ERROR_WANT_READ:
                            write_blocked_on_read = 1;
                            break;

                            /* Some other error */
                        default:
                            FD_ZERO(&fdread);
                            FD_ZERO(&fdwrite);
                            throw SmtpException(SmtpException::SSL_PROBLEM);
                    }

                }
            }

            FD_ZERO(&fdwrite);
            FD_ZERO(&fdread);
        }

        void SmtpServer::init_open_ssl()
        {
            SSL_library_init();
            SSL_load_error_strings();
            m_ctx = SSL_CTX_new(SSLv23_client_method());
            if (m_ctx == nullptr)
                throw SmtpException(SmtpException::SSL_PROBLEM);
        }

        void SmtpServer::open_ssl_connect()
        {
            if (m_ctx == nullptr)
                throw SmtpException(SmtpException::SSL_PROBLEM);
            m_ssl = SSL_new(m_ctx);
            if (m_ssl == nullptr)
                throw SmtpException(SmtpException::SSL_PROBLEM);
            SSL_set_fd(m_ssl, (int) m_socket);
            SSL_set_mode(m_ssl, SSL_MODE_AUTO_RETRY);

            int res = 0;
            fd_set fdwrite;
            fd_set fdread;
            int write_blocked = 0;
            int read_blocked = 0;

            timeval time;
            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            while (true) {
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdread);

                if (write_blocked)
                    FD_SET(m_socket, &fdwrite);
                if (read_blocked)
                    FD_SET(m_socket, &fdread);

                if (write_blocked || read_blocked) {
                    write_blocked = 0;
                    read_blocked = 0;
                    if ((res = select(m_socket + 1, &fdread, &fdwrite, nullptr, &time)) == SOCKET_ERROR) {
                        FD_ZERO(&fdwrite);
                        FD_ZERO(&fdread);
                        throw SmtpException(SmtpException::WSA_SELECT);
                    }
                    if (!res) {
                        //timeout
                        FD_ZERO(&fdwrite);
                        FD_ZERO(&fdread);
                        throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
                    }
                }
                res = SSL_connect(m_ssl);
                switch (SSL_get_error(m_ssl, res)) {
                    case SSL_ERROR_NONE:
                        FD_ZERO(&fdwrite);
                        FD_ZERO(&fdread);
                        return;
                        break;

                    case SSL_ERROR_WANT_WRITE:
                        write_blocked = 1;
                        break;

                    case SSL_ERROR_WANT_READ:
                        read_blocked = 1;
                        break;

                    default:
                        FD_ZERO(&fdwrite);
                        FD_ZERO(&fdread);
                        throw SmtpException(SmtpException::SSL_PROBLEM);
                }
            }
        }

        void SmtpServer::cleanup_open_ssl()
        {
            if (m_ssl != nullptr) {
                SSL_shutdown(m_ssl);  /* send SSL/TLS close_notify */
                SSL_free(m_ssl);
                m_ssl = nullptr;
            }
            if (m_ctx != nullptr) {
                SSL_CTX_free(m_ctx);
                m_ctx = nullptr;
                ERR_remove_state(0);
                ERR_free_strings();
                EVP_cleanup();
                CRYPTO_cleanup_all_ex_data();
            }
        }
    }//namespace smtp
}//namespace md
