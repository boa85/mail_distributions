#pragma once

#include "smtp_exception.hpp"
#include "../../tools/service/service.hpp"
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
#endif
#define TIME_IN_SEC        3*60    // how long client will wait for server response in non-blocking mode
#define BUFFER_SIZE        10240    // send_data and RecvData buffers sizes
#define MSG_SIZE_IN_MB    25        // the maximum size of the message with all attachments
#define COUNTER_VALUE    100        // how many times program will try to receive data
namespace md
{
    using namespace service;
    namespace smtp
    {
        const char BOUNDARY_TEXT[] = "__MESSAGE__ID__54yg6f6h6y456345";

        enum SMTP_XPRIORITY
        {
            XPRIORITY_HIGH = 2,
            XPRIORITY_NORMAL = 3,
            XPRIORITY_LOW = 4
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

        Command_Entry *find_command_entry(SMTP_COMMAND command);

// A simple string match
        bool is_keyword_supported(const char *response, const char *keyword);

        unsigned char *char2uchar(const char *strIn);


    }//namespace smtp
}//namespace md

