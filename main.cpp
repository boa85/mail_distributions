#include <thread>
#include <iostream>
#include <syslog.h>
#include "core/smtp/smtp_server.hpp"
#include "core/database/pg_backend.hpp"
#include "tools/args_parser/argument_parser.hpp"

using namespace md::db;
//using namespace md::smtp;
using namespace md::argument_parser;

void send_mail(const StringList &list, const std::string &smtp_host, unsigned smtp_port)
{
    /*if (list.size() != 10) {
        throw std::logic_error("invalid database record");
    }*/
/*
    try {
//        SmtpServer mail;
//        mail.init(list, smtp_host, smtp_port);
//        mail.send_mail();
    }
    catch (SmtpException &e) {
        write_sys_log(e.get_error_message());
        std::cout << "Error: " << e.get_error_message().c_str() << ".\n";
    }
*/
}

void
test_send_mail(const PgbPtr &pgBackend, const std::string &smtp_host, unsigned smtp_port)
{
    auto conn = pgBackend->connection();

    std::string demo = "SELECT * FROM core.emails; ";
    PQsendQuery(conn->connection().get(), demo.c_str());

    StringListArray query_result;

    while (auto result = PQgetResult(conn->connection().get())) {
        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result)) {
            auto pq_tuples_count = PQntuples(result);
            auto pq_fields_count = PQnfields(result);
            for (auto jdx = 0; jdx < pq_tuples_count; ++jdx) {
                StringList list;
                for (auto idx = 0; idx < pq_fields_count; ++idx) {
                    auto item = PQgetvalue(result, jdx, idx);
                    list.emplace_back(item);
                }
                send_mail(list, smtp_host, smtp_port);
                query_result.emplace_back(list);
            }
        }

        if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
            std::cout << PQresultErrorMessage(result) << std::endl;
        }

        PQclear(result);
    }

    for (const auto &list:query_result) {
        std::cout << list << std::endl;
    }

    pgBackend->free_connection(conn);

}


int main(int argc, char const *argv[])
{

    bool bError = false;

    try
    {
        CSmtp mail;

#define test_gmail_tls

#if defined(test_gmail_tls)
        mail.SetSMTPServer("smtp.yandex.ru",587);
        mail.SetSecurityType(USE_TLS);
#elif defined(test_gmail_ssl)
        mail.SetSMTPServer("smtp.gmail.com",465);
		mail.SetSecurityType(USE_SSL);
#elif defined(test_hotmail_TLS)
		mail.SetSMTPServer("smtp.live.com",25);
		mail.SetSecurityType(USE_TLS);
#elif defined(test_aol_tls)
		mail.SetSMTPServer("smtp.aol.com",587);
		mail.SetSecurityType(USE_TLS);
#elif defined(test_yahoo_ssl)
		mail.SetSMTPServer("plus.smtp.mail.yahoo.com",465);
		mail.SetSecurityType(USE_SSL);
#endif

        mail.SetLogin("belaev.oa@yandex.ru");
        mail.SetPassword("vusvifwyflgcxlvo");
        mail.SetSenderName("User");
        mail.SetSenderMail("belaev.oa@yandex.ru");
        mail.SetReplyTo("belaev.oa@yandex.ru");
        mail.SetSubject("The message");
        mail.AddRecipient("boa.freelance@gmail.com");
        mail.SetXPriority(XPRIORITY_NORMAL);
        mail.SetXMailer("The Bat! (v3.02) Professional");
        mail.AddMsgLine("Hello,");
        mail.AddMsgLine("I think this shit works.");
        mail.AddMsgLine("...");
        mail.AddMsgLine("How are you today?");
        mail.AddMsgLine("");
        mail.AddMsgLine("Regards");
        mail.ModMsgLine(5,"regards");
        mail.DelMsgLine(2);
        mail.AddMsgLine("User");
        mail.Send();
    }
    catch(ECSmtp e)
    {
        std::cout << "Error: " << e.GetErrorText().c_str() << ".\n";
        bError = true;
    }
    if(!bError)
        std::cout << "Mail was send successfully.\n";
    return 0;
/*
    try {
        auto parser = std::make_shared<ArgumentParser>();
        parser->start_parsing(argc, argv);
        std::string path_to_server_conf = parser->path_to_server_conf();
        std::string path_to_db_conf = parser->path_to_db_conf();
        SysErrorCode error_code;
        auto db_conf = read_config(path_to_db_conf, CONFIG_TYPE::DATABASE, error_code);
        auto pg_backend = std::make_shared<PGBackend>();
        auto server_conf = dynamic_cast<ServerConfig *>(read_config(path_to_server_conf, CONFIG_TYPE::SERVER,
                                                                    error_code).get());

        auto smtp_host = server_conf->m_domain;
        auto smtp_port = server_conf->m_port;
        pg_backend->setup_connection(db_conf);
        std::vector<std::shared_ptr<std::thread>> vec;
        for (size_t i = 0; i < 1; ++i) {
            vec.push_back(std::make_shared<std::thread>(std::thread(test_send_mail, pg_backend, smtp_host, smtp_port)));
        }

        for (auto &i : vec) {
            i.get()->join();
        }
        return 0;
    }
   */
/* catch (SmtpException &e) {
        openlog("mail_distribution", LOG_PERROR | LOG_PID, LOG_USER);
//        syslog(LOG_NOTICE, "error %s", e.get_error_message().c_str());
        closelog();
    }*//*

    catch (std::exception &e) {
        openlog("mail_distribution", LOG_PERROR | LOG_PID, LOG_USER);
        syslog(LOG_NOTICE, "error %s", e.what());
        closelog();
        std::cerr << e.what() << std::endl;
    }
*/

}
