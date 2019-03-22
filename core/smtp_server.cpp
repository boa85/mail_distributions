
#include <zconf.h>
#include "smtp_server.hpp"
#include "base_64.hpp"

namespace md
{
    namespace smtp
    {
        CSmtp::CSmtp()
        {
            m_xPriority = XPRIORITY_NORMAL;
            m_smtp_server_port = 0;


            if((m_receive_buffer = new char[BUFFER_SIZE]) == NULL)
                throw ECSmtp(ECSmtp::LACK_OF_MEMORY);

            if((send_buffer = new char[BUFFER_SIZE]) == NULL)
                throw ECSmtp(ECSmtp::LACK_OF_MEMORY);
        }

        CSmtp::~CSmtp()
        {
            if(send_buffer)
            {
                delete[] send_buffer;
                send_buffer = NULL;
            }
            if(m_receive_buffer)
            {
                delete[] m_receive_buffer;
                m_receive_buffer = NULL;
            }

        }

        void CSmtp::add_attachment(const char *path)
        {
            assert(path);
            m_attachments.insert(m_attachments.end(),path);
        }

        void CSmtp::add_recipient(const char *email, const char *name)
        {
            if(!email)
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENT_MAIL);

            Recipient recipient;
            recipient.m_mail.insert(0,email);
            name!=NULL ? recipient.m_name.insert(0,name) : recipient.m_name.insert(0,"");

            m_recipients.insert(m_recipients.end(), recipient);
        }

        void CSmtp::add_CC_recipient(const char *email, const char *name)
        {
            if(!email)
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENT_MAIL);

            Recipient recipient;
            recipient.m_mail.insert(0,email);
            name!=NULL ? recipient.m_name.insert(0,name) : recipient.m_name.insert(0,"");

            m_cc_recipients.insert(m_cc_recipients.end(), recipient);
        }


        void CSmtp::add_BCC_recipient(const char *email, const char *name)
        {
            if(!email)
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENT_MAIL);

            Recipient recipient;
            recipient.m_mail.insert(0,email);
            name!=NULL ? recipient.m_name.insert(0,name) : recipient.m_name.insert(0,"");

            m_bcc_recipients.insert(m_bcc_recipients.end(), recipient);
        }


        void CSmtp::add_msg_line(const char *text)
        {
            m_message_body.insert(m_message_body.end(),text);
        }


        void CSmtp::delete_message_line(unsigned int line_number)
        {
            if(line_number > m_message_body.size())
                throw ECSmtp(ECSmtp::OUT_OF_MSG_RANGE);
            m_message_body.erase(m_message_body.begin()+line_number);
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

        void CSmtp::modify_message_line(unsigned int line_number, const char *text)
        {
            if(text)
            {
                if(line_number > m_message_body.size())
                    throw ECSmtp(ECSmtp::OUT_OF_MSG_RANGE);
                m_message_body.at(line_number) = std::string(text);
            }
        }

        void CSmtp::send_mail()
        {
            unsigned int i,rcpt_count,res,FileId;
            char *FileBuf = NULL, *FileName = NULL;
            FILE* hFile = NULL;
            unsigned long int FileSize,TotalSize,MsgPart;
            bool bAccepted;

            // ***** CONNECTING TO SMTP SERVER *****

            // connecting to remote host:
            if( (m_socket = connect_remote_server(m_smtp_server_name.c_str(), m_smtp_server_port)) == INVALID_SOCKET )
                throw ECSmtp(ECSmtp::WSA_INVALID_SOCKET);

            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 220:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::SERVER_NOT_READY);
                }
            }while(!bAccepted);

            // EHLO <SP> <domain> <CRLF>
            sprintf(send_buffer,"EHLO %s\r\n", get_local_hostname()!=NULL ? m_local_hostname.c_str() : "domain");
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 250:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_EHLO);
                }
            }while(!bAccepted);

            // AUTH <SP> LOGIN <CRLF>
            strcpy(send_buffer,"AUTH LOGIN\r\n");
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 250:
                        break;
                    case 334:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_AUTH_LOGIN);
                }
            }while(!bAccepted);

            // send login:
            if(!m_login.size())
                throw ECSmtp(ECSmtp::UNDEF_LOGIN);
            std::string encoded_login = base64_encode(reinterpret_cast<const unsigned char*>(m_login.c_str()),m_login.size());
            sprintf(send_buffer,"%s\r\n",encoded_login.c_str());
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 334:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::UNDEF_XYZ_RESPONSE);
                }
            }while(!bAccepted);

            // send password:
            if(!m_password.size())
                throw ECSmtp(ECSmtp::UNDEF_PASSWORD);
            std::string encoded_password = base64_encode(reinterpret_cast<const unsigned char*>(m_password.c_str()),m_password.size());
            sprintf(send_buffer,"%s\r\n",encoded_password.c_str());
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
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
            }while(!bAccepted);

            // ***** SENDING E-MAIL *****

            // MAIL <SP> FROM:<reverse-path> <CRLF>
            if(!m_mail_from.size())
                throw ECSmtp(ECSmtp::UNDEF_MAIL_FROM);
            sprintf(send_buffer,"MAIL FROM:<%s>\r\n",m_mail_from.c_str());
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 250:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_MAIL_FROM);
                }
            }while(!bAccepted);

            // RCPT <SP> TO:<forward-path> <CRLF>
            if(!(rcpt_count = m_recipients.size()))
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENTS);
            for(i=0;i<m_recipients.size();i++)
            {
                sprintf(send_buffer,"RCPT TO:<%s>\r\n",(m_recipients.at(i).m_mail).c_str());
                send_data();
                bAccepted = false;
                do
                {
                    receive_data();
                    switch(smtp_xyz_digits())
                    {
                        case 250:
                            bAccepted = true;
                            break;
                        default:
                            rcpt_count--;
                    }
                }while(!bAccepted);
            }
            if(rcpt_count <= 0)
                throw ECSmtp(ECSmtp::COMMAND_RCPT_TO);

            for(i=0;i<m_cc_recipients.size();i++)
            {
                sprintf(send_buffer,"RCPT TO:<%s>\r\n",(m_cc_recipients.at(i).m_mail).c_str());
                send_data();
                bAccepted = false;
                do
                {
                    receive_data();
                    switch(smtp_xyz_digits())
                    {
                        case 250:
                            bAccepted = true;
                            break;
                        default:
                            ; // not necessary to throw
                    }
                }while(!bAccepted);
            }

            for(i=0;i<m_bcc_recipients.size();i++)
            {
                sprintf(send_buffer,"RCPT TO:<%s>\r\n",(m_bcc_recipients.at(i).m_mail).c_str());
                send_data();
                bAccepted = false;
                do
                {
                    receive_data();
                    switch(smtp_xyz_digits())
                    {
                        case 250:
                            bAccepted = true;
                            break;
                        default:
                            ; // not necessary to throw
                    }
                }while(!bAccepted);
            }

            // DATA <CRLF>
            strcpy(send_buffer,"DATA\r\n");
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 354:
                        bAccepted = true;
                        break;
                    case 250:
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_DATA);
                }
            }while(!bAccepted);

            // send header(s)
            format_header(send_buffer);
            send_data();

            // send text message
            if(get_line_count())
            {
                for(i=0;i< get_line_count();i++)
                {
                    sprintf(send_buffer,"%s\r\n", get_message_line_text(i));
                    send_data();
                }
            }
            else
            {
                sprintf(send_buffer,"%s\r\n"," ");
                send_data();
            }

            // next goes attachments (if they are)
            if((FileBuf = new char[55]) == NULL)
                throw ECSmtp(ECSmtp::LACK_OF_MEMORY);

            if((FileName = new char[255]) == NULL)
                throw ECSmtp(ECSmtp::LACK_OF_MEMORY);

            TotalSize = 0;
            for(FileId=0;FileId<m_attachments.size();FileId++)
            {
                strcpy(FileName,m_attachments[FileId].c_str());

                sprintf(send_buffer,"--%s\r\n",BOUNDARY_TEXT);
                strcat(send_buffer,"Content-Type: application/x-msdownload; name=\"");
                strcat(send_buffer,&FileName[m_attachments[FileId].find_last_of("\\") + 1]);
                strcat(send_buffer,"\"\r\n");
                strcat(send_buffer,"Content-Transfer-Encoding: base64\r\n");
                strcat(send_buffer,"Content-Disposition: attachment; filename=\"");
                strcat(send_buffer,&FileName[m_attachments[FileId].find_last_of("\\") + 1]);
                strcat(send_buffer,"\"\r\n");
                strcat(send_buffer,"\r\n");

                send_data();

                // opening the file:
                hFile = fopen(FileName,"rb");
                if(hFile == NULL)
                    throw ECSmtp(ECSmtp::FILE_NOT_EXIST);

                // checking file size:
                FileSize = 0;
                while(!feof(hFile))
                    FileSize += fread(FileBuf,sizeof(char),54,hFile);
                TotalSize += FileSize;

                // sending the file:
                if(TotalSize/1024 > MSG_SIZE_IN_MB*1024)
                    throw ECSmtp(ECSmtp::MSG_TOO_BIG);
                else
                {
                    fseek (hFile,0,SEEK_SET);

                    MsgPart = 0;
                    for(i=0;i<FileSize/54+1;i++)
                    {
                        res = fread(FileBuf,sizeof(char),54,hFile);
                        MsgPart ? strcat(send_buffer,base64_encode(reinterpret_cast<const unsigned char*>(FileBuf),res).c_str())
                                : strcpy(send_buffer,base64_encode(reinterpret_cast<const unsigned char*>(FileBuf),res).c_str());
                        strcat(send_buffer,"\r\n");
                        MsgPart += res + 2;
                        if(MsgPart >= BUFFER_SIZE/2)
                        { // sending part of the message
                            MsgPart = 0;
                            send_data(); // FileBuf, FileName, fclose(hFile);
                        }
                    }
                    if(MsgPart)
                    {
                        send_data(); // FileBuf, FileName, fclose(hFile);
                    }
                }
                fclose(hFile);
            }
            delete[] FileBuf;
            delete[] FileName;

            // sending last message block (if there is one or more attachments)
            if(m_attachments.size())
            {
                sprintf(send_buffer,"\r\n--%s--\r\n",BOUNDARY_TEXT);
                send_data();
            }

            // <CRLF> . <CRLF>
            strcpy(send_buffer,"\r\n.\r\n");
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 250:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::MSG_BODY_ERROR);
                }
            }while(!bAccepted);

            // ***** CLOSING CONNECTION *****

            // QUIT <CRLF>
            strcpy(send_buffer,"QUIT\r\n");
            send_data();
            bAccepted = false;
            do
            {
                receive_data();
                switch(smtp_xyz_digits())
                {
                    case 221:
                        bAccepted = true;
                        break;
                    default:
                        throw ECSmtp(ECSmtp::COMMAND_QUIT);
                }
            }while(!bAccepted);

            close(m_socket);
            m_socket = NULL;
        }

        SOCKET CSmtp::connect_remote_server(const char *server, const unsigned short port)
        {
            unsigned short nPort = 0;
            LPSERVENT lpServEnt;
            SOCKADDR_IN sockAddr;
            unsigned long ul = 1;
            fd_set fdwrite,fdexcept;
            timeval timeout;
            int res = 0;

            timeout.tv_sec = TIME_IN_SEC;
            timeout.tv_usec = 0;

            SOCKET hSocket = INVALID_SOCKET;

            if((hSocket = socket(PF_INET, SOCK_STREAM,0)) == INVALID_SOCKET)
                throw ECSmtp(ECSmtp::WSA_INVALID_SOCKET);

            if(port != 0)
                nPort = htons(port);
            else
            {
                lpServEnt = getservbyname("mail", 0);
                if (lpServEnt == NULL)
                    nPort = htons(25);
                else
                    nPort = lpServEnt->s_port;
            }

            sockAddr.sin_family = AF_INET;
            sockAddr.sin_port = nPort;
            if((sockAddr.sin_addr.s_addr = inet_addr(server)) == INADDR_NONE)
            {
                LPHOSTENT host;

                host = gethostbyname(server);
                if (host)
                    memcpy(&sockAddr.sin_addr,host->h_addr_list[0],host->h_length);
                else
                {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_GETHOSTBY_NAME_ADDR);
                }
            }

            // start non-blocking mode for socket:
            if(ioctl(hSocket,FIONBIO, (unsigned long*)&ul) == SOCKET_ERROR)
            {
                close(hSocket);
                throw ECSmtp(ECSmtp::WSA_IOCTLSOCKET);
            }

            if(connect(hSocket,(LPSOCKADDR)&sockAddr,sizeof(sockAddr)) == SOCKET_ERROR)
            {
                if(errno != EINPROGRESS)
                {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_CONNECT);
                }
            }
            else
                return hSocket;

            while(true)
            {
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdexcept);

                FD_SET(hSocket,&fdwrite);
                FD_SET(hSocket,&fdexcept);

                if((res = select(hSocket+1,NULL,&fdwrite,&fdexcept,&timeout)) == SOCKET_ERROR)
                {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }

                if(!res)
                {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::SELECT_TIMEOUT);
                }
                if(res && FD_ISSET(hSocket,&fdwrite))
                    break;
                if(res && FD_ISSET(hSocket,&fdexcept))
                {
                    close(hSocket);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }
            } // while

            FD_CLR(hSocket,&fdwrite);
            FD_CLR(hSocket,&fdexcept);

            return hSocket;
        }

        int CSmtp::smtp_xyz_digits()
        {
            assert(m_receive_buffer);
            if(m_receive_buffer == NULL)
                return 0;
            return (m_receive_buffer[0]-'0')*100 + (m_receive_buffer[1]-'0')*10 + m_receive_buffer[2]-'0';
        }

        void CSmtp::format_header(char *header)
        {
            char month[][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
            size_t i;
            std::string to;
            std::string cc;
            std::string bcc;
            time_t rawtime;
            struct tm* timeinfo;

            // date/time check
            if(time(&rawtime) > 0)
                timeinfo = localtime(&rawtime);
            else
                throw ECSmtp(ECSmtp::TIME_ERROR);

            // check for at least one recipient
            if(m_recipients.size())
            {
                for (i=0;i<m_recipients.size();i++)
                {
                    if(i > 0)
                        to.append(",");
                    to += m_recipients[i].m_name;
                    to.append("<");
                    to += m_recipients[i].m_mail;
                    to.append(">");
                }
            }
            else
                throw ECSmtp(ECSmtp::UNDEF_RECIPIENTS);

            if(m_cc_recipients.size())
            {
                for (i=0;i<m_cc_recipients.size();i++)
                {
                    if(i > 0)
                        cc. append(",");
                    cc += m_cc_recipients[i].m_name;
                    cc.append("<");
                    cc += m_cc_recipients[i].m_mail;
                    cc.append(">");
                }
            }

            if(!m_bcc_recipients.empty())
            {
                for (i=0;i<m_bcc_recipients.size();i++)
                {
                    if(i > 0)
                        bcc.append(",");
                    bcc += m_bcc_recipients[i].m_name;
                    bcc.append("<");
                    bcc += m_bcc_recipients[i].m_mail;
                    bcc.append(">");
                }
            }

            // Date: <SP> <dd> <SP> <mon> <SP> <yy> <SP> <hh> ":" <mm> ":" <ss> <SP> <zone> <CRLF>
            sprintf(header,"Date: %d %s %d %d:%d:%d\r\n",	timeinfo->tm_mday,
                    month[timeinfo->tm_mon],
                    timeinfo->tm_year+1900,
                    timeinfo->tm_hour,
                    timeinfo->tm_min,
                    timeinfo->tm_sec);

            // From: <SP> <sender>  <SP> "<" <sender-email> ">" <CRLF>
            if(!m_mail_from.size())
                throw ECSmtp(ECSmtp::UNDEF_MAIL_FROM);
            strcat(header,"From: ");
            if(m_name_from.size())
                strcat(header, m_name_from.c_str());
            strcat(header," <");
            if(m_name_from.size())
                strcat(header,m_mail_from.c_str());
            else
                strcat(header,"mail@domain.com");
            strcat(header, ">\r\n");

            // X-Mailer: <SP> <xmailer-app> <CRLF>
            if(m_mailer.size())
            {
                strcat(header,"X-Mailer: ");
                strcat(header, m_mailer.c_str());
                strcat(header, "\r\n");
            }

            // Reply-To: <SP> <reverse-path> <CRLF>
            if(m_reply_to.size())
            {
                strcat(header, "Reply-To: ");
                strcat(header, m_reply_to.c_str());
                strcat(header, "\r\n");
            }

            // X-Priority: <SP> <number> <CRLF>
            switch(m_xPriority)
            {
                case XPRIORITY_HIGH:
                    strcat(header,"X-Priority: 2 (High)\r\n");
                    break;
                case XPRIORITY_NORMAL:
                    strcat(header,"X-Priority: 3 (Normal)\r\n");
                    break;
                case XPRIORITY_LOW:
                    strcat(header,"X-Priority: 4 (Low)\r\n");
                    break;
                default:
                    strcat(header,"X-Priority: 3 (Normal)\r\n");
            }

            // To: <SP> <remote-user-mail> <CRLF>
            strcat(header,"To: ");
            strcat(header, to.c_str());
            strcat(header, "\r\n");

            // Cc: <SP> <remote-user-mail> <CRLF>
            if(m_cc_recipients.size())
            {
                strcat(header,"Cc: ");
                strcat(header, cc.c_str());
                strcat(header, "\r\n");
            }

            if(m_bcc_recipients.size())
            {
                strcat(header,"Bcc: ");
                strcat(header, bcc.c_str());
                strcat(header, "\r\n");
            }

            // Subject: <SP> <subject-text> <CRLF>
            if(!m_subject.size())
                strcat(header, "Subject:  ");
            else
            {
                strcat(header, "Subject: ");
                strcat(header, m_subject.c_str());
            }
            strcat(header, "\r\n");

            // MIME-Version: <SP> 1.0 <CRLF>
            strcat(header,"MIME-Version: 1.0\r\n");
            if(!m_attachments.size())
            { // no attachments
                strcat(header,"Content-type: text/plain; charset=US-ASCII\r\n");
                strcat(header,"Content-Transfer-Encoding: 7bit\r\n");
                strcat(send_buffer,"\r\n");
            }
            else
            { // there is one or more attachments
                strcat(header,"Content-Type: multipart/mixed; boundary=\"");
                strcat(header,BOUNDARY_TEXT);
                strcat(header,"\"\r\n");
                strcat(header,"\r\n");
                // first goes text message
                strcat(send_buffer,"--");
                strcat(send_buffer,BOUNDARY_TEXT);
                strcat(send_buffer,"\r\n");
                strcat(send_buffer,"Content-type: text/plain; charset=US-ASCII\r\n");
                strcat(send_buffer,"Content-Transfer-Encoding: 7bit\r\n");
                strcat(send_buffer,"\r\n");
            }

            // done
        }

        void CSmtp::receive_data()
        {
            int res,i = 0;
            fd_set fdread;
            timeval time;

            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            assert(m_receive_buffer);

            if(m_receive_buffer == NULL)
                throw ECSmtp(ECSmtp::RECVBUF_IS_EMPTY);

            while(1)
            {
                FD_ZERO(&fdread);

                FD_SET(m_socket,&fdread);

                if((res = select(m_socket+1, &fdread, NULL, NULL, &time)) == SOCKET_ERROR)
                {
                    FD_CLR(m_socket,&fdread);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }

                if(!res)
                {
                    //timeout
                    FD_CLR(m_socket,&fdread);
                    throw ECSmtp(ECSmtp::SERVER_NOT_RESPONDING);
                }

                if(res && FD_ISSET(m_socket,&fdread))
                {
                    if(i >= BUFFER_SIZE)
                    {
                        FD_CLR(m_socket,&fdread);
                        throw ECSmtp(ECSmtp::LACK_OF_MEMORY);
                    }
                    if(recv(m_socket,&m_receive_buffer[i++],1,0) == SOCKET_ERROR)
                    {
                        FD_CLR(m_socket,&fdread);
                        throw ECSmtp(ECSmtp::WSA_RECV);
                    }
                    if(m_receive_buffer[i-1]=='\n')
                    {
                        m_receive_buffer[i] = '\0';
                        break;
                    }
                }
            }

            FD_CLR(m_socket,&fdread);
        }

        void CSmtp::send_data()
        {
            int idx = 0,res,nLeft = strlen(send_buffer);
            fd_set fdwrite;
            timeval time;

            time.tv_sec = TIME_IN_SEC;
            time.tv_usec = 0;

            assert(send_buffer);

            if(send_buffer == NULL)
                throw ECSmtp(ECSmtp::SENDBUF_IS_EMPTY);

            while(1)
            {
                FD_ZERO(&fdwrite);

                FD_SET(m_socket,&fdwrite);

                if((res = select(m_socket+1,NULL,&fdwrite,NULL,&time)) == SOCKET_ERROR)
                {
                    FD_CLR(m_socket,&fdwrite);
                    throw ECSmtp(ECSmtp::WSA_SELECT);
                }

                if(!res)
                {
                    //timeout
                    FD_CLR(m_socket,&fdwrite);
                    throw ECSmtp(ECSmtp::SERVER_NOT_RESPONDING);
                }

                if(res && FD_ISSET(m_socket,&fdwrite))
                {
                    if(nLeft > 0)
                    {
                        if((res = send(m_socket,&send_buffer[idx],nLeft,0)) == SOCKET_ERROR)
                        {
                            FD_CLR(m_socket,&fdwrite);
                            throw ECSmtp(ECSmtp::WSA_SEND);
                        }
                        if(!res)
                            break;
                        nLeft -= res;
                        idx += res;
                    }
                    else
                        break;
                }
            }

            FD_CLR(m_socket,&fdwrite);
        }

        const char* CSmtp::get_local_hostname() const
        {
            char* str = NULL;

            if((str = new char[255]) == NULL)
                throw ECSmtp(ECSmtp::LACK_OF_MEMORY);
            if(gethostname(str,255) == SOCKET_ERROR)
            {
                delete[] str;
                throw ECSmtp(ECSmtp::WSA_HOSTNAME);
            }
            delete[] str;
            return m_local_hostname.c_str();
        }

        unsigned int CSmtp::get_recipient_count() const
        {
            return m_recipients.size();
        }

        unsigned int CSmtp::get_BCC_recipient_count() const
        {
            return m_bcc_recipients.size();
        }

        unsigned int CSmtp::get_CC_recipient_count() const
        {
            return m_cc_recipients.size();
        }

        const char* CSmtp::get_reply_to() const
        {
            return m_reply_to.c_str();
        }

        const char* CSmtp::get_mail_from() const
        {
            return m_mail_from.c_str();
        }

        const char* CSmtp::get_sender_name() const
        {
            return m_name_from.c_str();
        }

        const char* CSmtp::get_subject() const
        {
            return m_subject.c_str();
        }

        const char* CSmtp::get_xmailer() const
        {
            return m_mailer.c_str();
        }

        CSmptXPriority CSmtp::get_xpriority() const
        {
            return m_xPriority;
        }

        const char* CSmtp::get_message_line_text(unsigned int line_number) const
        {
            if(line_number > m_message_body.size())
                throw ECSmtp(ECSmtp::OUT_OF_MSG_RANGE);
            return m_message_body.at(line_number).c_str();
        }

        unsigned int CSmtp::get_line_count() const
        {
            return m_message_body.size();
        }

        void CSmtp::set_xpriority(CSmptXPriority priority)
        {
            m_xPriority = priority;
        }

        void CSmtp::set_reply_to(const char *ReplyTo)
        {
            //m_reply_to.erase();
            m_reply_to.insert(0,ReplyTo);
        }

        void CSmtp::set_sender_mail(const char *EMail)
        {
            m_mail_from.erase();
            m_mail_from.insert(0,EMail);
        }

        void CSmtp::set_sender_name(const char *Name)
        {
            m_name_from.erase();
            m_name_from.insert(0,Name);
        }

        void CSmtp::set_subject(const char *Subject)
        {
            m_subject.erase();
            m_subject.insert(0,Subject);
        }

        void CSmtp::set_xmailer(const char *XMailer)
        {
            m_mailer.erase();
            m_mailer.insert(0,XMailer);
        }

        void CSmtp::set_login(const char *Login)
        {
            m_login.erase();
            m_login.insert(0,Login);
        }

        void CSmtp::set_password(const char *Password)
        {
            m_password.erase();
            m_password.insert(0,Password);
        }

        void CSmtp::set_smtp_server(const char *server, const unsigned short port)
        {
            m_smtp_server_port = port;
            m_smtp_server_name.erase();
            m_smtp_server_name.insert(0,server);
        }

        std::string ECSmtp::get_error_message() const
        {
            switch(m_error_code)
            {
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
                case ECSmtp::LACK_OF_MEMORY:
                    return "Lack of memory";
                case ECSmtp::TIME_ERROR:
                    return "time() error";
                case ECSmtp::RECVBUF_IS_EMPTY:
                    return "m_receive_buffer is empty";
                case ECSmtp::SENDBUF_IS_EMPTY:
                    return "send_buffer is empty";
                case ECSmtp::OUT_OF_MSG_RANGE:
                    return "Specified line number is out of message size";
                default:
                    return "Undefined error id";
            }
        }

    }
}
