#include <utility>
#include <zconf.h>
#include <vector>
#include "smtp_server.hpp"
#include "../../tools/service/base_64.hpp"
#include "md_5.hpp"

namespace md::smtp
    {


    using namespace service;

        SmtpServer::SmtpServer()
                : m_security_type(DO_NOT_SET)
                  , m_xPriority(XPRIORITY_NORMAL)
                  , m_smtp_server_port(0)
                  , m_socket(INVALID_SOCKET)
                  , m_authenticate(true)
                  , m_ssl(nullptr)
                  , m_ssl_ctx(nullptr)
                  , m_bConnected(false)
                  , m_bHTML(false)
                  , m_read_receipt(false)
        {

            if ((m_receive_buffer = new char[BUFFER_SIZE]) == nullptr) {
                SmtpException e(SmtpException::BAD_MEMORY_ALLOCC);
                write_sys_log(e.get_error_message());
            }
            if ((m_send_buffer = new char[BUFFER_SIZE]) == nullptr) {
                SmtpException e(SmtpException::BAD_MEMORY_ALLOCC);
                write_sys_log(e.get_error_message());
            }
        }


        SmtpServer::~SmtpServer()
        {
            if (m_bConnected)
                disconnect_remote_server();
            if (m_send_buffer) {
                delete[] m_send_buffer;
                m_send_buffer = nullptr;
            }
            if (m_receive_buffer) {
                delete[] m_receive_buffer;
                m_receive_buffer = nullptr;
            }
            cleanup_openSSL();
        }


        void SmtpServer::add_attachment(std::string &&path)
        {
            m_attachments.insert(m_attachments.end(), std::move(path));
        }

        void SmtpServer::add_recipient(std::string email, std::string name)
        {
            if (!is_valid_email(email)) {
                SmtpException e(SmtpException::UNDEF_RECIPIENT_MAIL);
                write_sys_log(e.get_error_message());
                throw SmtpException(SmtpException::UNDEF_RECIPIENT_MAIL);
            }
            m_recipients.emplace_back(Recipient(std::move(email), std::move(name)));
        }

        void SmtpServer::add_CC_recipient(std::string email, std::string name)
        {
            if (!is_valid_email(email)) {
                SmtpException e(SmtpException::UNDEF_RECIPIENT_MAIL);
                write_sys_log(e.get_error_message());
                throw SmtpException(SmtpException::UNDEF_RECIPIENT_MAIL);
            }
            m_cc_recipients.emplace_back(Recipient(std::move(email), std::move(name)));
        }


        void SmtpServer::add_BCC_recipient(std::string email, std::string name)
        {
            if (!is_valid_email(email)) {
                SmtpException e(SmtpException::UNDEF_RECIPIENT_MAIL);
                write_sys_log(e.get_error_message());
                throw SmtpException(SmtpException::UNDEF_RECIPIENT_MAIL);
            }
            Recipient recipient(std::move(email), std::move(name));
            m_bcc_recipients.emplace_back(recipient);
        }


        void SmtpServer::add_msg_line(std::string text)
        {
            m_message_body.emplace_back(text);
        }


        void SmtpServer::delete_message_line(unsigned int line_number)
        {
            if (line_number > m_message_body.size()) {
                SmtpException e(SmtpException::OUT_OF_MSG_RANGE);
                write_sys_log(e.get_error_message());
                throw SmtpException(SmtpException::OUT_OF_MSG_RANGE);
            }
            m_message_body.erase(m_message_body.begin() + line_number);
        }


        void SmtpServer::delete_recipients()
        {
            m_recipients.clear();
        }


        void SmtpServer::delete_BCC_recipients()
        {
            m_bcc_recipients.clear();
        }

        void SmtpServer::delete_CC_recipients()
        {
            m_cc_recipients.clear();
        }

        void SmtpServer::delete_message_lines()
        {
            m_message_body.clear();
        }

        void SmtpServer::delete_attachments()
        {
            m_attachments.clear();
        }

        void SmtpServer::modify_message_line(unsigned int line_number, std::string text)
        {
            if (line_number > m_message_body.size()) {
                SmtpException e(SmtpException::OUT_OF_MSG_RANGE);
                write_sys_log(e.get_error_message());
                throw SmtpException(SmtpException::OUT_OF_MSG_RANGE);
            }
            m_message_body.at(line_number) = std::move(text);
        }

    void SmtpServer::clear_message()
    {
        delete_recipients();
        delete_BCC_recipients();
        delete_CC_recipients();
        delete_attachments();
        delete_message_lines();
    }

        void SmtpServer::send_mail()
        {

            size_t res;

            char *file_buffer = nullptr;

            FILE *hFile = nullptr;
            unsigned long int total_size;

            bool bAccepted;
            std::string::size_type position;

            // ***** CONNECTING TO SMTP SERVER *****

            // connecting to remote host if not already connected
            if (m_socket == INVALID_SOCKET) {
                if (!connect_remote_server(
                        m_smtp_server_name, m_smtp_server_port,
                        m_security_type, m_authenticate)) {
                    throw SmtpException(SmtpException::WSA_INVALID_SOCKET);
                }
            }
            try {
                //Allocate memory
                if ((file_buffer = new char[55]) == nullptr)
                    throw SmtpException(SmtpException::BAD_MEMORY_ALLOCC);

                //Check that any attachments specified can be opened
                if (!is_valid_attachments_size()) {
                    throw SmtpException(SmtpException::MSG_TOO_BIG);
                }

                // ***** SENDING E-MAIL *****

                // MAIL <SP> FROM:<reverse-path> <CRLF>
                if (m_mail_from.empty())
                    throw SmtpException(SmtpException::UNDEF_MAIL_FROM);
                Command_Entry *pEntry = find_command_entry(command_MAILFROM);
                snprintf(m_send_buffer, BUFFER_SIZE, "MAIL FROM:<%s>\r\n", m_mail_from.c_str());

                send_data(pEntry);
                receive_response(pEntry);

                // RCPT <SP> TO:<forward-path> <CRLF>
                if ((m_recipients.empty()))
                    throw SmtpException(SmtpException::UNDEF_RECIPIENTS);
                pEntry = find_command_entry(command_RCPTTO);
                auto send = [this, pEntry](const Recipients &recipients) {
                    for (const auto &recipient : recipients) {
                        snprintf(m_send_buffer, BUFFER_SIZE, "RCPT TO:<%s>\r\n", (recipient.m_mail).c_str());
                        send_data(pEntry);
                        receive_response(pEntry);
                    }
                };
                send(m_recipients);
                send(m_cc_recipients);
                send(m_bcc_recipients);
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
                if (!m_message_body.empty()) {
                    for (const auto &line : m_message_body) {
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", line.c_str());
                        send_data(pEntry);
                    }
                } else {
                    snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", " ");
                    send_data(pEntry);
                }

                for (auto file_id = 0; file_id < m_attachments.size(); file_id++) {
                    std::string filename;
                    auto pos = m_attachments[file_id].find_last_of('/');
                    filename = pos == string::npos ? m_attachments[file_id] : m_attachments[file_id].substr(pos + 1);
                    //RFC 2047 - Use UTF-8 charset,base64 encode.
                    std::string encoded_filename = "=?UTF-8?B?";
                    encoded_filename += base64_encode((unsigned char *) filename.c_str(), filename.size());
                    encoded_filename += "?=";

                    snprintf(m_send_buffer, BUFFER_SIZE, "--%s\r\n", BOUNDARY_TEXT);
                    strcat(m_send_buffer, "Content-Type: application/x-msdownload; name=\"");
                    strcat(m_send_buffer, encoded_filename.c_str());
                    strcat(m_send_buffer, "\"\r\n");
                    strcat(m_send_buffer, "Content-Transfer-Encoding: base64\r\n");
                    strcat(m_send_buffer, "Content-Disposition: attachment; filename=\"");
                    strcat(m_send_buffer, encoded_filename.c_str());
                    strcat(m_send_buffer, "\"\r\n");
                    strcat(m_send_buffer, "\r\n");

                    send_data(pEntry);

                    // opening the file:
                    hFile = fopen(m_attachments[file_id].c_str(), "rb");
                    if (hFile == nullptr)
                        throw SmtpException(SmtpException::FILE_NOT_EXIST);

                    // get file size:
                    fseek(hFile, 0, SEEK_END);
                    auto file_size = ftell(hFile);
                    fseek(hFile, 0, SEEK_SET);

                    auto message_part = 0;

                    for (auto i = 0; i < file_size / 54 + 1; i++) {
                        res = fread(file_buffer, sizeof(char), 54, hFile);
                        message_part ? strcat(m_send_buffer,
                                              base64_encode(reinterpret_cast<const unsigned char *>(file_buffer),
                                                            res).c_str())
                                     : strcpy(m_send_buffer,
                                              base64_encode(reinterpret_cast<const unsigned char *>(file_buffer),
                                                            res).c_str());
                        strcat(m_send_buffer, "\r\n");
                        message_part += res + 2;
                        if (message_part >= BUFFER_SIZE / 2) { // sending part of the message
                            message_part = 0;
                            send_data(pEntry); // FileBuf, filename, fclose(hFile);
                        }
                    }
                    if (message_part) {
                        send_data(pEntry); // FileBuf, filename, fclose(hFile);
                    }
                    fclose(hFile);
                }
                delete[] file_buffer;

                // sending last message block (if there is one or more attachments)
                if (!m_attachments.empty()) {
                    snprintf(m_send_buffer, BUFFER_SIZE, "\r\n--%s--\r\n", BOUNDARY_TEXT);
                    send_data(pEntry);
                }

                pEntry = find_command_entry(command_DATAEND);
                // <CRLF> . <CRLF>
                snprintf(m_send_buffer, BUFFER_SIZE, "\r\n.\r\n");
                send_data(pEntry);
                receive_response(pEntry);
            }
            catch (const SmtpException &exception) {
                fclose(hFile);
                delete[] file_buffer;
                disconnect_remote_server();
                throw;
            }

        }

    SOCKET SmtpServer::connect_remote_server(
            const char *server, const unsigned short port)
        {
            unsigned short port_number = 0;
            LPSERVENT lpservent;
            SOCKADDR_IN socket_address;
            unsigned long ul = 1;
            fd_set fd_except;
            fd_set fd_write;
            timeval timeout;
            int res = 0;

            timeout.tv_sec = TIME_IN_SEC;
            timeout.tv_usec = 0;

            SOCKET mSocket = INVALID_SOCKET;

            if ((mSocket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
                throw SmtpException(SmtpException::WSA_INVALID_SOCKET);

            if (port != 0)
                port_number = htons(port);
            else {
                lpservent = getservbyname("mail", nullptr);
                port_number = lpservent == nullptr ? htons(25) :
                              static_cast<unsigned short>(lpservent->s_port);
            }

            socket_address.sin_family = AF_INET;
            socket_address.sin_port = port_number;
            if ((socket_address.sin_addr.s_addr = inet_addr(server)) == INADDR_NONE) {
                LPHOSTENT host;

                host = gethostbyname(server);
                if (host)
                    memcpy(&socket_address.sin_addr, host->h_addr_list[0], static_cast<size_t>(host->h_length));
                else {
                    close(mSocket);
                    throw SmtpException(SmtpException::WSA_GETHOSTBY_NAME_ADDR);
                }
            }

            // start non-blocking mode for socket:
            if (ioctl(mSocket, FIONBIO, (unsigned long *) &ul) == SOCKET_ERROR) {
                close(mSocket);
                throw SmtpException(SmtpException::WSA_IOCTLSOCKET);
            }

            if (connect(mSocket, (LPSOCKADDR) &socket_address, sizeof(socket_address)) == SOCKET_ERROR) {
                if (errno != EINPROGRESS) {
                    close(mSocket);
                    throw SmtpException(SmtpException::WSA_CONNECT);
                }
            } else
                return mSocket;

            while (true) {
                FD_ZERO(&fd_write);
                FD_ZERO(&fd_except);

                FD_SET(mSocket, &fd_write);
                FD_SET(mSocket, &fd_except);

                if ((res = select(mSocket + 1, nullptr, &fd_write, &fd_except, &timeout)) == SOCKET_ERROR) {

                    close(mSocket);
                    throw SmtpException(SmtpException::WSA_SELECT);
                }

                if (!res) {
                    close(mSocket);
                    throw SmtpException(SmtpException::SELECT_TIMEOUT);
                }

                if (FD_ISSET(mSocket, &fd_write))
                    break;
            } // while

            FD_CLR(mSocket, &fd_write);
            FD_CLR(mSocket, &fd_except);

            return mSocket;
        }

        int SmtpServer::smtp_xyz_digits()
        {
            assert(m_receive_buffer);

            if (m_receive_buffer == nullptr)
                return 0;

            return (m_receive_buffer[0] - '0') * 100 +
                   (m_receive_buffer[1] - '0') * 10 + m_receive_buffer[2] - '0';
        }

        void SmtpServer::format_header(char *header)
        {
            char month[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

            std::string to;
            std::string cc;
            std::string bcc;
            time_t raw_time;
            struct tm *time_info;

            // date/time check
            if (time(&raw_time) > 0)
                time_info = localtime(&raw_time);
            else
                throw SmtpException(SmtpException::TIME_ERROR);

            // check for at least one recipient
            auto add_recipients2string = [this](const Recipients &recipients) -> std::string {
                std::string result;
                for (auto &recipient : recipients) {
                    result += recipient.m_name;
                    result.append("<");
                    result += recipient.m_mail;
                    result.append(">");
                }
                return result;
            };
            if (!m_recipients.empty()) {
                to += add_recipients2string(m_recipients);
            } else
                throw SmtpException(SmtpException::UNDEF_RECIPIENTS);

            cc += add_recipients2string(m_cc_recipients);
            bcc += add_recipients2string(m_bcc_recipients);


            // Date: <SP> <dd> <SP> <mon> <SP> <yy> <SP> <hh> ":" <mm> ":" <ss> <SP> <zone> <CRLF>
            sprintf(header, "Date: %d %s %d %d:%d:%d\r\n", time_info->tm_mday,
                    month[time_info->tm_mon],
                    time_info->tm_year + 1900,
                    time_info->tm_hour,
                    time_info->tm_min,
                    time_info->tm_sec);

            // From: <SP> <sender>  <SP> "<" <sender-email> ">" <CRLF>
            if (m_mail_from.empty())
                throw SmtpException(SmtpException::UNDEF_MAIL_FROM);

            strcat(header, "From: ");

            if (!m_name_from.empty())
                strcat(header, m_name_from.c_str());

            strcat(header, " <");

            strcat(header, !m_name_from.empty() ? m_mail_from.c_str() : "mail@domain.com");

            strcat(header, ">\r\n");

            // X-Mailer: <SP> <xmailer-app> <CRLF>
            if (!m_mailer.empty()) {
                strcat(header, "X-Mailer: ");
                strcat(header, m_mailer.c_str());
                strcat(header, "\r\n");
            }

            // Reply-To: <SP> <reverse-path> <CRLF>
            if (!m_reply_to.empty()) {
                strcat(header, "Reply-To: ");
                strcat(header, m_reply_to.c_str());
                strcat(header, "\r\n");
            }

            // X-Priority: <SP> <number> <CRLF>
            switch (m_xPriority) {
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
            if (!m_cc_recipients.empty()) {
                strcat(header, "Cc: ");
                strcat(header, cc.c_str());
                strcat(header, "\r\n");
            }

            if (!m_bcc_recipients.empty()) {
                strcat(header, "Bcc: ");
                strcat(header, bcc.c_str());
                strcat(header, "\r\n");
            }

            // Subject: <SP> <subject-text> <CRLF>
            if (m_subject.empty())
                strcat(header, "Subject:  ");
            else {
                strcat(header, "Subject: ");
                strcat(header, m_subject.c_str());
            }
            strcat(header, "\r\n");

            // MIME-Version: <SP> 1.0 <CRLF>
            strcat(header, "MIME-Version: 1.0\r\n");
            if (m_attachments.empty()) { // no attachments
                strcat(header, "Content-type: text/plain; charset=US-ASCII\r\n");
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
                strcat(m_send_buffer, "Content-type: text/plain; charset=US-ASCII\r\n");
                strcat(m_send_buffer, "Content-Transfer-Encoding: 7bit\r\n");
                strcat(m_send_buffer, "\r\n");
            }

            // done
        }

        void SmtpServer::receive_data(Command_Entry *pEntry)
        {
            if (m_ssl != nullptr) {
                receive_data_SSL(m_ssl, pEntry);
                return;
            }
            auto i = 0;
            int res = 0;
            fd_set fdSet;
            timeval time;

            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            assert(m_receive_buffer);

            if (m_receive_buffer == nullptr)
                throw SmtpException(SmtpException::RECVBUF_IS_EMPTY);

            while (true) {
                FD_ZERO(&fdSet);

                FD_SET(m_socket, &fdSet);

                if ((res = select(m_socket + 1, &fdSet, nullptr, nullptr, &time)) == SOCKET_ERROR) {
                    FD_CLR(m_socket, &fdSet);
                    throw SmtpException(SmtpException::WSA_SELECT);
                }

                if (!res) {
                    //timeout
                    FD_CLR(m_socket, &fdSet);
                    throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
                }

                if (FD_ISSET(m_socket, &fdSet)) {
                    if (i >= BUFFER_SIZE) {
                        FD_CLR(m_socket, &fdSet);
                        throw SmtpException(SmtpException::BAD_MEMORY_ALLOCC);
                    }
                    if (recv(m_socket, &m_receive_buffer[i++], 1, 0) == SOCKET_ERROR) {
                        FD_CLR(m_socket, &fdSet);
                        throw SmtpException(SmtpException::WSA_RECV);
                    }
                    if (m_receive_buffer[i - 1] == '\n') {
                        m_receive_buffer[i] = '\0';
                        break;
                    }
                }
            }

            FD_CLR(m_socket, &fdSet);
        }

        void SmtpServer::send_data(Command_Entry *pEntry)
        {
            if (m_ssl != nullptr) {
                send_data_SSL(m_ssl, pEntry);
                return;
            }
            auto n_left = strlen(m_send_buffer);
            auto res = 0;
            int idx = 0;
            fd_set fd_write;
            timeval time;

            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            assert(m_send_buffer);

            if (m_send_buffer == nullptr)
                throw SmtpException(SmtpException::SENDBUF_IS_EMPTY);

            while (true) {
                FD_ZERO(&fd_write);
                FD_SET(m_socket, &fd_write);

                if ((res = select(m_socket + 1, nullptr, &fd_write, nullptr, &time)) == SOCKET_ERROR) {

                    FD_CLR(m_socket, &fd_write);
                    throw SmtpException(SmtpException::WSA_SELECT);
                }

                if (res == 0) {
                    //timeout
                    FD_CLR(m_socket, &fd_write);
                    throw SmtpException(SmtpException::SERVER_NOT_RESPONDING);
                }

                if (FD_ISSET(m_socket, &fd_write)) {
                    if (n_left <= 0)
                        break;

                    if (n_left > 0) {
                        if (send(m_socket, &m_send_buffer[idx], n_left, 0) == SOCKET_ERROR) {
                            FD_CLR(m_socket, &fd_write);
                            throw SmtpException(SmtpException::WSA_SEND);
                        }
                        n_left -= res;
                        idx += res;
                    }
                }
            }

            FD_CLR(m_socket, &fd_write);
        }

        const char *SmtpServer::get_local_hostname()
        {

            char *str = nullptr;

            if ((str = new char[255]) == nullptr)
                throw SmtpException(SmtpException::BAD_MEMORY_ALLOCC);

            if (gethostname(str, 255) == SOCKET_ERROR) {
                delete[] str;
                throw SmtpException(SmtpException::WSA_HOSTNAME);
            }

            m_local_hostname = std::string(str);
            delete[] str;
            return m_local_hostname.c_str();
        }

        unsigned long SmtpServer::get_recipient_count() const
        {
            return m_recipients.size();
        }

        unsigned long SmtpServer::get_BCC_recipient_count() const
        {
            return m_bcc_recipients.size();
        }

        unsigned long SmtpServer::get_CC_recipient_count() const
        {
            return m_cc_recipients.size();
        }

        std::string SmtpServer::get_reply_to() const
        {
            return m_reply_to;
        }

        std::string SmtpServer::get_mail_from() const
        {
            return m_mail_from;
        }

        std::string SmtpServer::get_sender_name() const
        {
            return m_name_from;
        }

        std::string SmtpServer::get_subject() const
        {
            return m_subject;
        }

        std::string SmtpServer::get_xmailer() const
        {
            return m_mailer;
        }

        CSmptXPriority SmtpServer::get_xpriority() const
        {
            return m_xPriority;
        }

        std::string SmtpServer::get_message_line_text(unsigned int line_number) const
        {
            if (line_number > m_message_body.size())
                throw SmtpException(SmtpException::OUT_OF_MSG_RANGE);

            return m_message_body.at(line_number);
        }

        unsigned long SmtpServer::get_line_count() const
        {
            return m_message_body.size();
        }

        void SmtpServer::set_xpriority(CSmptXPriority priority)
        {
            m_xPriority = priority;
        }

        void SmtpServer::set_reply_to(std::string reply_to)
        {
            m_reply_to = std::move(reply_to);
        }

        void SmtpServer::set_sender_mail(std::string email)
        {
            m_mail_from = std::move(email);
        }

        void SmtpServer::set_sender_name(std::string name)
        {
            m_name_from = std::move(name);
        }

        void SmtpServer::set_subject(std::string subject)
        {
            m_subject = std::move(subject);
        }

        void SmtpServer::set_xmailer(std::string mailer)
        {
            m_mailer = std::move(mailer);
        }

        void SmtpServer::set_login(std::string login)
        {
            m_login = std::move(login);
        }

        void SmtpServer::set_password(std::string password)
        {
            m_password = std::move(password);
        }

        void SmtpServer::set_smtp_server(std::string server, unsigned int port, bool authenticate)
        {
            m_smtp_server_port = port;
            m_smtp_server_name = std::move(server);
            m_authenticate = authenticate;

        }

        void SmtpServer::init(const StringList &list, const std::string &smtp_host, unsigned smtp_port)
        {

            set_smtp_server(smtp_host, smtp_port, false);

            set_login(list[0]);
            set_password(list[1]);
            set_sender_name(list[2]);
            set_sender_mail(list[3]);

            set_reply_to(list[4]);
            set_subject((list[5]));
            add_recipient(list[6], "user");
            set_xmailer(list[7]);
            set_xpriority(static_cast<CSmptXPriority>(std::stoi(list[8])));
            add_msg_line(list[9]);
//            mail.add_msg_line("");
//            mail.add_msg_line("...");
//            mail.add_msg_line("How are you today?");
//            mail.add_msg_line("");
//            mail.add_msg_line("Regards");
////        mail.modMsgLine(5, "regards");
////        mail.delMsgLine(2);
////        mail.addMsgLine("User");
//
//            //mail.addAttachment("../test1.jpg");
//            //mail.addAttachment("c:\\test2.exe");

        }

        void SmtpServer::set_charset(const std::string &charset)
        {
            m_charset = charset;
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
                                            (line[begin] - '0') * 100 + (line[begin + 1] - '0') * 10 + line[begin + 2] -
                                            '0';
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
            snprintf(m_receive_buffer, BUFFER_SIZE, "%s", line.c_str());
            if (reply_code != pEntry->valid_reply_code) {
                throw SmtpException(pEntry->error);
            }
        }

        void SmtpServer::init_openSSL()
        {
            SSL_library_init();
            SSL_load_error_strings();
            m_ssl_ctx = SSL_CTX_new(SSLv23_client_method());
            if (m_ssl_ctx == NULL)
                throw SmtpException(SmtpException::SSL_PROBLEM);
        }

        void SmtpServer::openSSL_connect()
        {
            if (m_ssl_ctx == nullptr)
                throw SmtpException(SmtpException::SSL_PROBLEM);
            m_ssl = SSL_new(m_ssl_ctx);
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
                    if ((res = select(m_socket + 1, &fdread, &fdwrite, NULL, &time)) == SOCKET_ERROR) {

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

        void SmtpServer::cleanup_openSSL()
        {
            if (m_ssl != nullptr) {
                SSL_shutdown(m_ssl);  /* send SSL/TLS close_notify */
                SSL_free(m_ssl);
                m_ssl = nullptr;
            }
            if (m_ssl_ctx != nullptr) {
                SSL_CTX_free(m_ssl_ctx);
                m_ssl_ctx = nullptr;
//                ERR_remove_state(0);
//                ERR_free_strings();
                EVP_cleanup();
                CRYPTO_cleanup_all_ex_data();
            }
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

            if (m_receive_buffer == NULL)
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
                                throw SmtpException(SmtpException::BAD_MEMORY_ALLOCC);
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

        void SmtpServer::send_data_SSL(SSL *ssl, Command_Entry *pEntry)
        {
            int offset = 0;
            int res = 0;
            int nLeft = strlen(m_send_buffer);
            fd_set fdwrite;
            fd_set fdread;
            timeval time;

            int write_blocked_on_read = 0;

            time.tv_sec = pEntry->send_timeout;
            time.tv_usec = 0;

            assert(m_send_buffer);

            if (m_send_buffer == NULL)
                throw SmtpException(SmtpException::SENDBUF_IS_EMPTY);

            while (nLeft > 0) {
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdread);

                FD_SET(m_socket, &fdwrite);

                if (write_blocked_on_read) {
                    FD_SET(m_socket, &fdread);
                }

                if ((res = select(m_socket + 1, &fdread, &fdwrite, NULL, &time)) == SOCKET_ERROR) {

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

        SMTP_SECURITY_TYPE SmtpServer::get_security_type() const
        {
            return m_security_type;
        }

        void SmtpServer::set_security_type(SMTP_SECURITY_TYPE type)
        {
            m_security_type = type;
        }

    bool SmtpServer::connect_remote_server(const std::string &server
                                           , const unsigned short port
                                           , SMTP_SECURITY_TYPE security_type
                                           , bool authenticate
                                           , const char *login
                                           , const char *password)
        {

            unsigned short port_num = 0;
            LPSERVENT lpServEnt;
            SOCKADDR_IN sockAddr;
            unsigned long ul = 1;
            fd_set fdexcept;
            fd_set fdwrite;
            timeval timeout;
            int res = 0;

            try {
                timeout.tv_sec = TIME_IN_SEC;
                timeout.tv_usec = 0;

                m_socket = INVALID_SOCKET;

                if ((m_socket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
                    throw SmtpException(SmtpException::WSA_INVALID_SOCKET);

                if (port != 0)
                    port_num = htons(port);
                else {
                    lpServEnt = getservbyname("mail", nullptr);
                    if (lpServEnt == nullptr)
                        port_num = htons(25);
                    else
                        port_num = lpServEnt->s_port;
                }

                sockAddr.sin_family = AF_INET;
                sockAddr.sin_port = port_num;
                if ((sockAddr.sin_addr.s_addr = inet_addr(server.c_str())) == INADDR_NONE) {

                    LPHOSTENT host;

                    host = gethostbyname(server.c_str());
                    if (host)
                        memcpy(&sockAddr.sin_addr, host->h_addr_list[0], host->h_length);
                    else {
                        close(m_socket);
                        throw SmtpException(SmtpException::WSA_GETHOSTBY_NAME_ADDR);
                    }
                }

                // start non-blocking mode for socket:
                if (ioctl(m_socket, FIONBIO, (unsigned long *) &ul) == SOCKET_ERROR) {
                    close(m_socket);
                    throw SmtpException(SmtpException::WSA_IOCTLSOCKET);
                }

                if (connect(m_socket, (LPSOCKADDR) &sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
                    if (errno != EINPROGRESS) {
                        close(m_socket);
                        throw SmtpException(SmtpException::WSA_CONNECT);
                    }
                } else
                    return true;

                while (true) {
                    FD_ZERO(&fdwrite);
                    FD_ZERO(&fdexcept);

                    FD_SET(m_socket, &fdwrite);
                    FD_SET(m_socket, &fdexcept);

                    if ((res = select(m_socket + 1, NULL, &fdwrite, &fdexcept, &timeout)) == SOCKET_ERROR) {
                        close(m_socket);
                        throw SmtpException(SmtpException::WSA_SELECT);
                    }

                    if (!res) {
                        close(m_socket);
                        throw SmtpException(SmtpException::SELECT_TIMEOUT);
                    }
                    if (FD_ISSET(m_socket, &fdwrite))
                        break;
                    if (FD_ISSET(m_socket, &fdexcept)) {
                        close(m_socket);
                        throw SmtpException(SmtpException::WSA_SELECT);
                    }
                } // while

                FD_CLR(m_socket, &fdwrite);
                FD_CLR(m_socket, &fdexcept);

                if (security_type != DO_NOT_SET) {
                    set_security_type(security_type);
                }

                if (get_security_type() == USE_TLS || get_security_type() == USE_SSL) {
                    init_openSSL();
                    if (get_security_type() == USE_SSL) {
                        openSSL_connect();
                    }
                }

                Command_Entry *pEntry = find_command_entry(command_INIT);
                receive_response(pEntry);

                say_hello();

                if (get_security_type() == USE_TLS) {
                    start_tls();
                    say_hello();
                }

                if (authenticate && is_keyword_supported(m_receive_buffer, "AUTH")) {
                    if (login)
                        set_login(login);
                    if (m_login.empty())
                        throw SmtpException(SmtpException::UNDEF_LOGIN);

                    if (password)
                        set_password(password);

                    if (m_password.empty())
                        throw SmtpException(SmtpException::UNDEF_PASSWORD);

                    if (is_keyword_supported(m_receive_buffer, "LOGIN")) {
                        pEntry = find_command_entry(command_AUTHLOGIN);
                        snprintf(m_send_buffer, BUFFER_SIZE, "AUTH LOGIN\r\n");
                        send_data(pEntry);
                        receive_response(pEntry);

                        // send login:
                        std::string encoded_login = base64_encode(
                                reinterpret_cast<const unsigned char *>(m_login.c_str()), m_login.size());
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


                    } else if (is_keyword_supported(m_receive_buffer, "PLAIN")) {
                        pEntry = find_command_entry(command_AUTHPLAIN);
                        snprintf(m_send_buffer, BUFFER_SIZE, "%s^%s^%s", m_login.c_str(), m_login.c_str(),
                                 m_password.c_str());
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
                    } else if (is_keyword_supported(m_receive_buffer, "CRAM-MD5")) {
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

                        unsigned char *ustr_challenge = char2uchar(decoded_challenge.c_str());
                        unsigned char *ustr_password = char2uchar(m_password.c_str());
                        if (!ustr_challenge || !ustr_password)
                            throw SmtpException(SmtpException::BAD_LOGIN_PASS);

                        // if ustr_password is longer than 64 bytes reset it to ustr_password=MD5(ustr_password)
                        int passwordLength = m_password.size();
                        if (passwordLength > 64) {
                            MD5 md5password;
                            md5password.update(ustr_password, passwordLength);
                            md5password.finalize();
                            ustr_password = md5password.raw_digest();
                            passwordLength = 16;
                        }

                        //Storing ustr_password in pads
                        unsigned char ipad[65], opad[65];
                        memset(ipad, 0, 64);
                        memset(opad, 0, 64);
                        memcpy(ipad, ustr_password, passwordLength);
                        memcpy(opad, ustr_password, passwordLength);

                        // XOR ustr_password with ipad and opad values
                        for (int i = 0; i < 64; i++) {
                            ipad[i] ^= 0x36;
                            opad[i] ^= 0x5c;
                        }

                        //perform inner MD5
                        MD5 md5pass1;
                        md5pass1.update(ipad, 64);
                        md5pass1.update(ustr_challenge, decoded_challenge.size());
                        md5pass1.finalize();
                        unsigned char *ustrResult = md5pass1.raw_digest();

                        //perform outer MD5
                        MD5 md5pass2;
                        md5pass2.update(opad, 64);
                        md5pass2.update(ustrResult, 16);
                        md5pass2.finalize();
                        decoded_challenge = md5pass2.hex_digest();

                        delete[] ustr_challenge;
                        delete[] ustr_password;
                        delete[] ustrResult;

                        decoded_challenge = m_login + " " + decoded_challenge;
                        encoded_challenge = base64_encode(
                                reinterpret_cast<const unsigned char *>(decoded_challenge.c_str()),
                                decoded_challenge.size());

                        snprintf(m_send_buffer, BUFFER_SIZE, "%s\r\n", encoded_challenge.c_str());
                        pEntry = find_command_entry(command_PASSWORD);
                        send_data(pEntry);
                        receive_response(pEntry);
                    } else if (is_keyword_supported(m_receive_buffer, "DIGEST-MD5")) {
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
                        find = nonce.find('\"');
                        if (find < 0)
                            throw SmtpException(SmtpException::BAD_DIGEST_RESPONSE);
                        nonce = nonce.substr(0, find);

                        //Get the realm (optional)
                        std::string realm;
                        find = decoded_challenge.find("realm");
                        if (find >= 0) {
                            realm = decoded_challenge.substr(find + 7);
                            find = realm.find('\"');
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
                        if (!ustrRealm || !ustrUsername || !ustrPassword || !ustrNonce || !ustrCNonce || !ustrUri ||
                            !ustrNc || !ustrQop)
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
                        /*if (strstr(m_receive_buffer, "charset") >= 0)
                            snprintf(m_send_buffer, BUFFER_SIZE, "charset=utf-8,username=\"%s\"", m_login.c_str());*/
                        /*else */snprintf(m_send_buffer, BUFFER_SIZE, "username=\"%s\"", m_login.c_str());
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
                    } else
                        throw SmtpException(SmtpException::LOGIN_NOT_SUPPORTED);
                }
            }
            catch (const SmtpException &) {
                if (m_receive_buffer[0] == '5' && m_receive_buffer[1] == '3' && m_receive_buffer[2] == '0')
                    m_bConnected = false;

                disconnect_remote_server();
                throw;
            }

            return true;
        }

        void SmtpServer::say_hello()
        {
            auto pEntry = find_command_entry(command_EHLO);
            snprintf(m_send_buffer, BUFFER_SIZE, "EHLO %s\r\n",
                     get_local_hostname() != nullptr ? m_local_hostname.c_str() : "domain");
            send_data(pEntry);
            receive_response(pEntry);
            m_bConnected = true;
        }

        void SmtpServer::say_quit()
        {
            // ***** CLOSING CONNECTION *****

            auto pEntry = find_command_entry(command_QUIT);
            // QUIT <CRLF>
            snprintf(m_send_buffer, BUFFER_SIZE, "QUIT\r\n");
            m_bConnected = false;
            send_data(pEntry);
            receive_response(pEntry);
        }

        void SmtpServer::start_tls()
        {
            if (is_keyword_supported(m_receive_buffer, "STARTTLS")) {
                Command_Entry *pEntry = find_command_entry(command_STARTTLS);
                snprintf(m_send_buffer, BUFFER_SIZE, "STARTTLS\r\n");
                send_data(nullptr);
                receive_response(pEntry);
                openSSL_connect();
            } else {
                throw SmtpException(SmtpException::STARTTLS_NOT_SUPPORTED);
            }
        }

        void SmtpServer::disconnect_remote_server()
        {
            if (m_bConnected)
                say_quit();

            if (m_socket)
                close(m_socket);

            m_socket = INVALID_SOCKET;
        }

    bool SmtpServer::is_valid_attachments_size()
        {
            auto total_size = 0;
            for (unsigned int file_id = 0; file_id < m_attachments.size(); file_id++) {

                // opening the file:
                auto hFile = fopen(m_attachments[file_id].c_str(), "rb");
                if (hFile == nullptr) {
                    throw SmtpException(SmtpException::FILE_NOT_EXIST);
                }

                // checking file size:
                fseek(hFile, 0, SEEK_END);
                auto file_size = ftell(hFile);
                total_size += file_size;

                // sending the file:
                if (total_size / 1024 > MSG_SIZE_IN_MB * 1024) {
                    fclose(hFile);
                    return false;
                }
                fclose(hFile);
            }
            return true;
        }
    }
