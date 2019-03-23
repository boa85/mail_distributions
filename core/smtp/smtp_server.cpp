#include <utility>
#include <zconf.h>
#include "smtp_server.hpp"
#include "../../tools/service/base_64.hpp"
#include <string>

namespace md
{
    namespace smtp
    {
        using namespace service;

        CSmtp::CSmtp()
        {
            m_xPriority = XPRIORITY_NORMAL;
            m_smtp_server_port = 0;

            if ((m_receive_buffer = new char[BUFFER_SIZE]) == nullptr)
                throw ECSmtp(ECSmtp::BAD_MEMORY_ALLOCC);

            if ((m_send_buffer = new char[BUFFER_SIZE]) == nullptr)
                throw ECSmtp(ECSmtp::BAD_MEMORY_ALLOCC);

        }


        void CSmtp::add_attachment(std::string &&path)
        {
            m_attachments.insert(m_attachments.end(), std::move(path));
        }

        void CSmtp::add_recipient(std::string email, std::string name)
        {
            if (!is_valid_email(email))
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENT_MAIL);

            m_recipients.emplace_back(Recipient(std::move(email), std::move(name)));
        }

        void CSmtp::add_CC_recipient(std::string email, std::string name)
        {
            if (!is_valid_email(email))
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENT_MAIL);

            m_cc_recipients.emplace_back(Recipient(std::move(email), std::move(name)));
        }


        void CSmtp::add_BCC_recipient(std::string email, std::string name)
        {
            if (!is_valid_email(email))
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENT_MAIL);

            Recipient recipient(std::move(email), std::move(name));
            m_bcc_recipients.emplace_back(recipient);
        }


        void CSmtp::add_msg_line(std::string text)
        {
            m_message_body.emplace_back(text);
        }


        void CSmtp::delete_message_line(unsigned int line_number)
        {
            if (line_number > m_message_body.size())
                throw ECSmtp(ECSmtp::OUT_OF_MSG_RANGE);

            m_message_body.erase(m_message_body.begin() + line_number);
        }


        void CSmtp::delete_recipients()
        {
            m_recipients.clear();
        }


        void CSmtp::delete_BCC_recipients()
        {
            m_bcc_recipients.clear();
        }

        void CSmtp::delete_CC_recipients()
        {
            m_cc_recipients.clear();
        }

        void CSmtp::delete_message_lines()
        {
            m_message_body.clear();
        }

        void CSmtp::delete_attachments()
        {
            m_attachments.clear();
        }

        void CSmtp::modify_message_line(unsigned int line_number, std::string text)
        {
            if (line_number > m_message_body.size())
                throw ECSmtp(ECSmtp::OUT_OF_MSG_RANGE);

            m_message_body.at(line_number) = std::move(text);
        }

        void CSmtp::send_mail()
        {
            unsigned int file_id;
            size_t res;
            unsigned int recipient_count;

            char *filename = nullptr;
            char *file_buffer = nullptr;

            FILE *hFile = nullptr;
            unsigned long int message_part;
            unsigned long int total_size;
            unsigned long int file_size;
            bool bAccepted;

            // ***** CONNECTING TO SMTP SERVER *****

            // connecting to remote host:
            if ((m_socket =
                         connect_remote_server(m_smtp_server_name.c_str(), m_smtp_server_port)) == INVALID_SOCKET)
                throw ECSmtp(ECSmtp::WSA_INVALID_SOCKET);

            ECSmtp::CSmtpError error_code = ECSmtp::CSmtpError::CSMTP_NO_ERROR;
            auto try_receive_data = [this](const int response_code) -> bool {
                receive_data();
                return smtp_xyz_digits() == response_code;
            };
            error_code = ECSmtp::SERVER_NOT_READY;
            if (!try_receive_data(220)) {
                throw ECSmtp(error_code);
            }

            // EHLO <SP> <domain> <CRLF>
            sprintf(m_send_buffer, "EHLO %s\r\n",
                    get_local_hostname() != nullptr ? m_local_hostname.c_str() : "domain");
            send_data();

            if (!try_receive_data(250)) {
                throw ECSmtp(ECSmtp::COMMAND_EHLO);
            }

            // AUTH <SP> LOGIN <CRLF>
            strcpy(m_send_buffer, "AUTH LOGIN\r\n");
            send_data();
            bAccepted = false;

            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 250:
                        break;
                    case 334:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_AUTH_LOGIN);
                }
            } while (!bAccepted);

            // send login:
            if (m_login.empty())
                throw ECSmtp(ECSmtp::UNDEF_LOGIN);

            std::string encoded_login = base64_encode(reinterpret_cast<const unsigned char *>(m_login.c_str()),
                                                      m_login.size());
            sprintf(m_send_buffer, "%s\r\n", encoded_login.c_str());
            send_data();
            bAccepted = false;
            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 334:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::UNDEF_XYZ_RESPONSE);
                }
            } while (!bAccepted);

            // send password:
            if (m_password.empty())
                throw ECSmtp(ECSmtp::UNDEF_PASSWORD);

            std::string encoded_password = base64_encode(reinterpret_cast<const unsigned char *>(m_password.c_str()),
                                                         m_password.size());
            sprintf(m_send_buffer, "%s\r\n", encoded_password.c_str());
            send_data();
            bAccepted = false;
            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 235:
                        bAccepted = true;
                        break;
                    case 334:
                        break;
                    case 535:
                        throw ECSmtp(ECSmtp::BAD_LOGIN_PASS);
                    default:
                        throw ECSmtp(ECSmtp::UNDEF_XYZ_RESPONSE);
                }
            } while (!bAccepted);

            // ***** SENDING E-MAIL *****

            // MAIL <SP> FROM:<reverse-path> <CRLF>
            if (m_mail_from.empty())
                throw ECSmtp(ECSmtp::UNDEF_MAIL_FROM);

            sprintf(m_send_buffer, "MAIL FROM:<%s>\r\n", m_mail_from.c_str());
            send_data();
            bAccepted = false;
            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 250:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_MAIL_FROM);
                }
            } while (!bAccepted);

            // RCPT <SP> TO:<forward-path> <CRLF>
            if (!(recipient_count = static_cast<unsigned int>(m_recipients.size())))
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENTS);

            for (auto &m_recipient : m_recipients) {

                sprintf(m_send_buffer, "RCPT TO:<%s>\r\n", (m_recipient.m_mail).c_str());
                send_data();
                bAccepted = false;
                do {
                    receive_data();
                    switch (smtp_xyz_digits()) {
                        case 250:
                            bAccepted = true;
                            break;
                        default:
                            recipient_count--;
                    }
                } while (!bAccepted);
            }
            if (recipient_count <= 0)
                throw ECSmtp(ECSmtp::COMMAND_RCPT_TO);

            for (auto &m_cc_recipient : m_cc_recipients) {
                sprintf(m_send_buffer, "RCPT TO:<%s>\r\n", (m_cc_recipient.m_mail).c_str());
                send_data();
                bAccepted = false;
                do {
                    receive_data();
                    switch (smtp_xyz_digits()) {
                        case 250:
                            bAccepted = true;
                            break;
                        default:; // not necessary to throw
                    }
                } while (!bAccepted);
            }

            for (auto &m_bcc_recipient : m_bcc_recipients) {
                sprintf(m_send_buffer, "RCPT TO:<%s>\r\n", (m_bcc_recipient.m_mail).c_str());
                send_data();
                bAccepted = false;
                do {
                    receive_data();
                    switch (smtp_xyz_digits()) {
                        case 250:
                            bAccepted = true;
                            break;
                        default:; // not necessary to throw
                    }
                } while (!bAccepted);
            }

            // DATA <CRLF>
            strcpy(m_send_buffer, "DATA\r\n");
            send_data();
            bAccepted = false;
            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 354:
                        bAccepted = true;
                        break;
                    case 250:
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_DATA);
                }
            } while (!bAccepted);

            // send header(s)
            format_header(m_send_buffer);
            send_data();

            // send text message
            if (get_line_count()) {
                for (auto i = 0; i < get_line_count(); i++) {

                    sprintf(m_send_buffer, "%s\r\n", get_message_line_text(i).c_str());
                    send_data();
                }
            } else {
                sprintf(m_send_buffer, "%s\r\n", " ");
                send_data();
            }

            // next goes attachments (if they are)
            if ((file_buffer = new char[55]) == nullptr)
                throw ECSmtp(ECSmtp::BAD_MEMORY_ALLOCC);

            if ((filename = new char[255]) == nullptr)
                throw ECSmtp(ECSmtp::BAD_MEMORY_ALLOCC);

            total_size = 0;
            for (file_id = 0; file_id < m_attachments.size(); file_id++) {

                strcpy(filename, m_attachments[file_id].c_str());

                sprintf(m_send_buffer, "--%s\r\n", BOUNDARY_TEXT);
                strcat(m_send_buffer, "Content-Type: application/x-msdownload; name=\"");
                strcat(m_send_buffer, &filename[m_attachments[file_id].find_last_of("\\") + 1]);
                strcat(m_send_buffer, "\"\r\n");
                strcat(m_send_buffer, "Content-Transfer-Encoding: base64\r\n");
                strcat(m_send_buffer, "Content-Disposition: attachment; filename=\"");
                strcat(m_send_buffer, &filename[m_attachments[file_id].find_last_of("\\") + 1]);
                strcat(m_send_buffer, "\"\r\n");
                strcat(m_send_buffer, "\r\n");

                send_data();

                // opening the file:
                hFile = fopen(filename, "rb");
                if (hFile == nullptr)
                    throw ECSmtp(ECSmtp::FILE_NOT_EXIST);

                // checking file size:
                file_size = 0;
                while (!feof(hFile))
                    file_size += fread(file_buffer, sizeof(char), 54, hFile);

                total_size += file_size;

                // sending the file:
                if (total_size / 1024 > MSG_SIZE_IN_MB * 1024)
                    throw ECSmtp(ECSmtp::MSG_TOO_BIG);
                else {
                    fseek(hFile, 0, SEEK_SET);

                    message_part = 0;
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
                            send_data(); // file_buffer, filename, fclose(hFile);
                        }
                    }
                    if (message_part) {
                        send_data(); // file_buffer, filename, fclose(hFile);
                    }
                }
                fclose(hFile);
            }
            delete[] file_buffer;
            delete[] filename;

            // sending last message block (if there is one or more attachments)
            if (!m_attachments.empty()) {
                sprintf(m_send_buffer, "\r\n--%s--\r\n", BOUNDARY_TEXT);
                send_data();
            }

            // <CRLF> . <CRLF>
            strcpy(m_send_buffer, "\r\n.\r\n");
            send_data();
            bAccepted = false;
            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 250:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::MSG_BODY_ERROR);
                }
            } while (!bAccepted);

            // ***** CLOSING CONNECTION *****

            // QUIT <CRLF>
            strcpy(m_send_buffer, "QUIT\r\n");
            send_data();
            bAccepted = false;
            do {
                receive_data();
                switch (smtp_xyz_digits()) {
                    case 221:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_QUIT);
                }
            } while (!bAccepted);

            close(m_socket);
            m_socket = NULL;
        }

        SOCKET CSmtp::connect_remote_server(const char *server, const unsigned short port)
        {
            unsigned short nPort = 0;
            LPSERVENT lpServEnt;
            SOCKADDR_IN socket_address;
            unsigned long ul = 1;
            fd_set fd_except;
            fd_set fd_write;
            timeval timeout;
            int res = 0;

            timeout.tv_sec = TIME_IN_SEC;
            timeout.tv_usec = 0;

            SOCKET hSocket = INVALID_SOCKET;

            if ((hSocket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
                throw ECSmtp(ECSmtp::WSA_INVALID_SOCKET);

            if (port != 0)
                nPort = htons(port);
            else {
                lpServEnt = getservbyname("mail", nullptr);
                nPort = lpServEnt == nullptr ? htons(25) : static_cast<unsigned short>(lpServEnt->s_port);
            }

            socket_address.sin_family = AF_INET;
            socket_address.sin_port = nPort;
            if ((socket_address.sin_addr.s_addr = inet_addr(server)) == INADDR_NONE) {
                LPHOSTENT host;

                host = gethostbyname(server);
                if (host)
                    memcpy(&socket_address.sin_addr, host->h_addr_list[0], static_cast<size_t>(host->h_length));
                else {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_GETHOSTBY_NAME_ADDR);
                }
            }

            // start non-blocking mode for socket:
            if (ioctl(hSocket, FIONBIO, (unsigned long *) &ul) == SOCKET_ERROR) {
                close(hSocket);
                throw ECSmtp(ECSmtp::WSA_IOCTLSOCKET);
            }

            if (connect(hSocket, (LPSOCKADDR) &socket_address, sizeof(socket_address)) == SOCKET_ERROR) {
                if (errno != EINPROGRESS) {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_CONNECT);
                }
            } else
                return hSocket;

            while (true) {
                FD_ZERO(&fd_write);
                FD_ZERO(&fd_except);

                FD_SET(hSocket, &fd_write);
                FD_SET(hSocket, &fd_except);

                if ((res = select(hSocket + 1, nullptr, &fd_write, &fd_except, &timeout)) == SOCKET_ERROR) {

                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }

                if (!res) {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::SELECT_TIMEOUT);
                }

                if (FD_ISSET(hSocket, &fd_write))
                    break;

                if (FD_ISSET(hSocket, &fd_except)) {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }
            } // while

            FD_CLR(hSocket, &fd_write);
            FD_CLR(hSocket, &fd_except);

            return hSocket;
        }

        int CSmtp::smtp_xyz_digits()
        {
            assert(m_receive_buffer);

            if (m_receive_buffer == nullptr)
                return 0;

            return (m_receive_buffer[0] - '0') * 100 +
                   (m_receive_buffer[1] - '0') * 10 + m_receive_buffer[2] - '0';
        }

        void CSmtp::format_header(char *header)
        {
            char month[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

            std::string to;
            std::string cc;
            std::string bcc;
            time_t rawtime;
            struct tm *timeinfo;

            // date/time check
            if (time(&rawtime) > 0)
                timeinfo = localtime(&rawtime);
            else
                throw ECSmtp(ECSmtp::TIME_ERROR);

            // check for at least one recipient
            if (!m_recipients.empty()) {
                for (auto &m_recipient : m_recipients) {
                    to += m_recipient.m_name;
                    to.append("<");
                    to += m_recipient.m_mail;
                    to.append(">");
                }
            } else
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENTS);

            if (!m_cc_recipients.empty()) {
                for (auto &m_cc_recipient : m_cc_recipients) {
                    cc += m_cc_recipient.m_name;
                    cc.append("<");
                    cc += m_cc_recipient.m_mail;
                    cc.append(">");
                }
            }

            if (!m_bcc_recipients.empty()) {
                for (auto &m_bcc_recipient : m_bcc_recipients) {
                    bcc += m_bcc_recipient.m_name;
                    bcc.append("<");
                    bcc += m_bcc_recipient.m_mail;
                    bcc.append(">");
                }
            }

            // Date: <SP> <dd> <SP> <mon> <SP> <yy> <SP> <hh> ":" <mm> ":" <ss> <SP> <zone> <CRLF>
            sprintf(header, "Date: %d %s %d %d:%d:%d\r\n", timeinfo->tm_mday,
                    month[timeinfo->tm_mon],
                    timeinfo->tm_year + 1900,
                    timeinfo->tm_hour,
                    timeinfo->tm_min,
                    timeinfo->tm_sec);

            // From: <SP> <sender>  <SP> "<" <sender-email> ">" <CRLF>
            if (m_mail_from.empty())
                throw ECSmtp(ECSmtp::UNDEF_MAIL_FROM);

            strcat(header, "From: ");

            if (!m_name_from.empty())
                strcat(header, m_name_from.c_str());
            strcat(header, " <");

            if (!m_name_from.empty())
                strcat(header, m_mail_from.c_str());
            else
                strcat(header, "mail@domain.com");

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

        void CSmtp::receive_data()
        {
            auto i = 0;
            int res = 0;
            fd_set fdSet;
            timeval time;

            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            assert(m_receive_buffer);

            if (m_receive_buffer == nullptr)
                throw ECSmtp(ECSmtp::RECVBUF_IS_EMPTY);

            while (true) {
                FD_ZERO(&fdSet);

                FD_SET(m_socket, &fdSet);

                if ((res = select(m_socket + 1, &fdSet, nullptr, nullptr, &time)) == SOCKET_ERROR) {
                    FD_CLR(m_socket, &fdSet);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }

                if (!res) {
                    //timeout
                    FD_CLR(m_socket, &fdSet);
                    throw ECSmtp(ECSmtp::SERVER_NOT_RESPONDING);
                }

                if (FD_ISSET(m_socket, &fdSet)) {
                    if (i >= BUFFER_SIZE) {
                        FD_CLR(m_socket, &fdSet);
                        throw ECSmtp(ECSmtp::BAD_MEMORY_ALLOCC);
                    }
                    if (recv(m_socket, &m_receive_buffer[i++], 1, 0) == SOCKET_ERROR) {
                        FD_CLR(m_socket, &fdSet);
                        throw ECSmtp(ECSmtp::WSA_RECV);
                    }
                    if (m_receive_buffer[i - 1] == '\n') {
                        m_receive_buffer[i] = '\0';
                        break;
                    }
                }
            }

            FD_CLR(m_socket, &fdSet);
        }

        void CSmtp::send_data()
        {
            auto n_left = strlen(m_send_buffer);
            auto res = 0;
            int idx = 0;
            fd_set fd_write;
            timeval time;

            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            assert(m_send_buffer);

            if (m_send_buffer == nullptr)
                throw ECSmtp(ECSmtp::SENDBUF_IS_EMPTY);

            while (true) {
                FD_ZERO(&fd_write);

                FD_SET(m_socket, &fd_write);

                if ((res = select(m_socket + 1, nullptr, &fd_write, nullptr, &time)) == SOCKET_ERROR) {

                    FD_CLR(m_socket, &fd_write);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }

                if (res == 0) {
                    //timeout
                    FD_CLR(m_socket, &fd_write);
                    throw ECSmtp(ECSmtp::SERVER_NOT_RESPONDING);
                }

                if (FD_ISSET(m_socket, &fd_write)) {
                    if (n_left <= 0)
                        break;

                    if (n_left > 0)
                    {
                        if ((res = static_cast<int>(send(m_socket, &m_send_buffer[idx], n_left, 0))) == SOCKET_ERROR) {
                            FD_CLR(m_socket, &fd_write);
                            throw ECSmtp(ECSmtp::WSA_SEND);
                        }
                        if (res == 0)
                            break;

                        n_left -= res;
                        idx += res;
                    }
                }
            }

            FD_CLR(m_socket, &fd_write);
        }

        const char *CSmtp::get_local_hostname() const
        {
            char *str = nullptr;

            if ((str = new char[255]) == nullptr)
                throw ECSmtp(ECSmtp::BAD_MEMORY_ALLOCC);

            if (gethostname(str, 255) == SOCKET_ERROR) {
                delete[] str;
                throw ECSmtp(ECSmtp::WSA_HOSTNAME);
            }

            delete[] str;
            return m_local_hostname.c_str();
        }

        unsigned long CSmtp::get_recipient_count() const
        {
            return m_recipients.size();
        }

        unsigned long CSmtp::get_BCC_recipient_count() const
        {
            return m_bcc_recipients.size();
        }

        unsigned long CSmtp::get_CC_recipient_count() const
        {
            return m_cc_recipients.size();
        }

        std::string CSmtp::get_reply_to() const
        {
            return m_reply_to;
        }

        std::string CSmtp::get_mail_from() const
        {
            return m_mail_from;
        }

        std::string CSmtp::get_sender_name() const
        {
            return m_name_from;
        }

        std::string CSmtp::get_subject() const
        {
            return m_subject;
        }

        std::string CSmtp::get_xmailer() const
        {
            return m_mailer;
        }

        CSmptXPriority CSmtp::get_xpriority() const
        {
            return m_xPriority;
        }

        std::string CSmtp::get_message_line_text(unsigned int line_number) const
        {
            if (line_number > m_message_body.size())
                throw ECSmtp(ECSmtp::OUT_OF_MSG_RANGE);

            return m_message_body.at(line_number);
        }

        unsigned long CSmtp::get_line_count() const
        {
            return m_message_body.size();
        }

        void CSmtp::set_xpriority(CSmptXPriority priority)
        {
            m_xPriority = priority;
        }

        void CSmtp::set_reply_to(std::string reply_to)
        {
            m_reply_to = std::move(reply_to);
        }

        void CSmtp::set_sender_mail(std::string email)
        {
            m_mail_from = std::move(email);
        }

        void CSmtp::set_sender_name(const char *Name)
        {
            m_name_from.erase();
            m_name_from.insert(0, Name);
        }

        void CSmtp::set_subject(std::string subject)
        {
            m_subject = std::move(subject);
        }

        void CSmtp::set_xmailer(std::string mailer)
        {
            m_mailer = std::move(mailer);
        }

        void CSmtp::set_login(std::string login)
        {
            m_login = std::move(login);
        }

        void CSmtp::set_password(std::string password)
        {
            m_password = std::move(password);
        }

        void CSmtp::set_smtp_server(std::string server, const unsigned short port)
        {
            m_smtp_server_port = port;
            m_smtp_server_name = std::move(server);
        }

        std::string ECSmtp::get_error_message() const
        {
            switch (m_error_code) {
                case ECSmtp::CSMTP_NO_ERROR:
                    return "";
                case ECSmtp::WSA_STARTUP:
                    return "Unable to initialise winsock2";
                case ECSmtp::WSA_VER:
                    return "Wrong version of the winsock2";
                case ECSmtp::WSA_SEND:
                    return "Function send() failed";
                case ECSmtp::WSA_RECV:
                    return "Function recv() failed";
                case ECSmtp::WSA_CONNECT:
                    return "Function connect failed";
                case ECSmtp::WSA_GETHOSTBY_NAME_ADDR:
                    return "Unable to determine remote server";
                case ECSmtp::WSA_INVALID_SOCKET:
                    return "Invalid winsock2 socket";
                case ECSmtp::WSA_HOSTNAME:
                    return "Function hostname() failed";
                case ECSmtp::WSA_IOCTLSOCKET:
                    return "Function ioctlsocket() failed";
                case ECSmtp::BAD_IPV4_ADDR:
                    return "Improper IPv4 address";
                case ECSmtp::UNDEF_MSG_HEADER:
                    return "Undefined message header";
                case ECSmtp::UNDEF_MAIL_FROM:
                    return "Undefined mail sender";
                case ECSmtp::UNDEF_SUBJECT:
                    return "Undefined message subject";
                case ECSmtp::UNDEF_RECIPIENTS:
                    return "Undefined at least one reciepent";
                case ECSmtp::UNDEF_RECIPIENT_MAIL:
                    return "Undefined recipent mail";
                case ECSmtp::UNDEF_LOGIN:
                    return "Undefined user login";
                case ECSmtp::UNDEF_PASSWORD:
                    return "Undefined user password";
                case ECSmtp::COMMAND_MAIL_FROM:
                    return "Server returned error after sending MAIL FROM";
                case ECSmtp::COMMAND_EHLO:
                    return "Server returned error after sending EHLO";
                case ECSmtp::COMMAND_AUTH_LOGIN:
                    return "Server returned error after sending AUTH LOGIN";
                case ECSmtp::COMMAND_DATA:
                    return "Server returned error after sending DATA";
                case ECSmtp::COMMAND_QUIT:
                    return "Server returned error after sending QUIT";
                case ECSmtp::COMMAND_RCPT_TO:
                    return "Server returned error after sending RCPT TO";
                case ECSmtp::MSG_BODY_ERROR:
                    return "Error in message body";
                case ECSmtp::CONNECTION_CLOSED:
                    return "Server has closed the connection";
                case ECSmtp::SERVER_NOT_READY:
                    return "Server is not ready";
                case ECSmtp::SERVER_NOT_RESPONDING:
                    return "Server not responding";
                case ECSmtp::FILE_NOT_EXIST:
                    return "File not exist";
                case ECSmtp::MSG_TOO_BIG:
                    return "Message is too big";
                case ECSmtp::BAD_LOGIN_PASS:
                    return "Bad login or password";
                case ECSmtp::UNDEF_XYZ_RESPONSE:
                    return "Undefined xyz SMTP response";
                case ECSmtp::BAD_MEMORY_ALLOCC:
                    return "Lack of memory";
                case ECSmtp::TIME_ERROR:
                    return "time() error";
                case ECSmtp::RECVBUF_IS_EMPTY:
                    return "m_receive_buffer is empty";
                case ECSmtp::SENDBUF_IS_EMPTY:
                    return "m_send_buffer is empty";
                case ECSmtp::OUT_OF_MSG_RANGE:
                    return "Specified line number is out of message size";
                default:
                    return "Undefined error id";
            }
        }

    }
}
