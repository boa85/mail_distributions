#include <thread>
#include <iostream>
#include <syslog.h>
#include "core/smtp/smtp_server.hpp"
#include "core/database/pg_backend.hpp"
#include "tools/args_parser/argument_parser.hpp"
#include <chrono>
#include <ctime>
using namespace md::db;
using namespace md::smtp;
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
        SmtpServer mail;

#define test_gmail_tls

#if defined(test_gmail_tls)
        mail.init_smtp_server("smtp.yandex.ru", 587);
        mail.set_security_type(USE_TLS);
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


        std::vector<std::string> v = {"test", "jan", "febr", "marz", "apr", "mai"};

        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();
        for (auto i = 0; i < 1; ++i) {
            mail.set_login("belaev.oa@yandex.ru");
            mail.set_password("vusvifwyflgcxlvo");
            mail.set_sender_name("User");
            mail.set_sender_mail("belaev.oa@yandex.ru");
            mail.set_reply_to("belaev.oa@yandex.ru");
            mail.set_subject("The message");
            mail.add_recipient("belaev.oa@yandex.ru");
            mail.set_xpriority(XPRIORITY_NORMAL);
            mail.set_xmailer("The Bat! (v3.02) Professional");
            mail.add_message_line(v[i%6].c_str());
            mail.add_message_line("I think this shit works.");

            mail.add_message_line("User");
            mail.send_mail();
        }

        end = std::chrono::system_clock::now();

        int elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>
                (end-start).count();
        std::time_t end_time = std::chrono::system_clock::to_time_t(end);

        std::cout << "Вычисления закончены в " << std::ctime(&end_time)
                  << "Время выполнения: " << elapsed_seconds << "s\n";
    }
    catch(SmtpException e)
    {
        std::cout << "Error: " << e.get_error_message().c_str() << ".\n";
        bError = true;
    }
    if(!bError)
        std::cout << "m_mail was send successfully.\n";
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
