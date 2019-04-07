#include <assert.h>
#include <cstring>
#include "smtp_common.hpp"
#include "smtp_server.hpp"
namespace md
{
    using namespace service;
    namespace smtp
    {
        Command_Entry command_list[] =
                {
                        {command_INIT,          0,      5 * 60,  220, SmtpException::SERVER_NOT_RESPONDING},
                        {command_EHLO,          5 * 60, 5 * 60,  250, SmtpException::COMMAND_EHLO},
                        {command_AUTHPLAIN,     5 * 60, 5 * 60,  235, SmtpException::COMMAND_AUTH_PLAIN},
                        {command_AUTHLOGIN,     5 * 60, 5 * 60,  334, SmtpException::COMMAND_AUTH_LOGIN},
                        {command_AUTHCRAMMD5,   5 * 60, 5 * 60,  334, SmtpException::COMMAND_AUTH_CRAMMD5},
                        {command_AUTHDIGESTMD5, 5 * 60, 5 * 60,  334, SmtpException::COMMAND_AUTH_DIGESTMD5},
                        {command_DIGESTMD5,     5 * 60, 5 * 60,  335, SmtpException::COMMAND_DIGESTMD5},
                        {command_USER,          5 * 60, 5 * 60,  334, SmtpException::UNDEF_XYZ_RESPONSE},
                        {command_PASSWORD,      5 * 60, 5 * 60,  235, SmtpException::BAD_LOGIN_PASS},
                        {command_MAILFROM,      5 * 60, 5 * 60,  250, SmtpException::COMMAND_MAIL_FROM},
                        {command_RCPTTO,        5 * 60, 5 * 60,  250, SmtpException::COMMAND_RCPT_TO},
                        {command_DATA,          5 * 60, 2 * 60,  354, SmtpException::COMMAND_DATA},
                        {command_DATABLOCK,     3 *
                                                60,     0,       0,   SmtpException::COMMAND_DATABLOCK},    // Here the valid_reply_code is set to zero because there are no replies when sending data blocks
                        {command_DATAEND,       3 * 60, 10 * 60, 250, SmtpException::MSG_BODY_ERROR},
                        {command_QUIT,          5 * 60, 5 * 60,  221, SmtpException::COMMAND_QUIT},
                        {command_STARTTLS,      5 * 60, 5 * 60,  220, SmtpException::COMMAND_EHLO_STARTTLS}
                };

        Command_Entry *find_command_entry(SMTP_COMMAND command)
        {
            Command_Entry *pEntry = nullptr;
            for (auto & item : command_list) {
                if (item.command == command) {
                    pEntry = &item;
                    break;
                }
            }
            assert(pEntry != nullptr);
            return pEntry;
        }


        bool is_keyword_supported(const char *response, const char *keyword)
        {
            assert(response != nullptr && keyword != nullptr);
            if (response == nullptr || keyword == nullptr)
                return false;
            int res_len = strlen(response);
            int key_len = strlen(keyword);
            if (res_len < key_len)
                return false;
            int pos = 0;
            for (; pos < res_len - key_len + 1; ++pos) {
                if (_strnicmp(keyword, response + pos, key_len) == 0) {
                    if (pos > 0 &&
                        (response[pos - 1] == '-' ||
                         response[pos - 1] == ' ' ||
                         response[pos - 1] == '=')) {
                        if (pos + key_len < res_len) {
                            if (response[pos + key_len] == ' ' ||
                                response[pos + key_len] == '=') {
                                return true;
                            } else if (pos + key_len + 1 < res_len) {
                                if (response[pos + key_len] == '\r' &&
                                    response[pos + key_len + 1] == '\n') {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            return false;
        }



        unsigned char *char2uchar(const char *strIn)
        {
            unsigned char *strOut;

            unsigned long length,
                    i;


            length = strlen(strIn);

            strOut = new unsigned char[length + 1];
            if (!strOut) return nullptr;

            for (i = 0; i < length; i++) strOut[i] = (unsigned char) strIn[i];
            strOut[length] = '\0';

            return strOut;
        }

    }//namespace smtp
}//namespace md

