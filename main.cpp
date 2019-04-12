#include <thread>
#include <iostream>
#include <syslog.h>
#include "core/smtp/smtp_server.hpp"
#include "core/database/pg_backend.hpp"
#include "tools/args_parser/argument_parser.hpp"
#include "core/database/db_tools.hpp"
#include "core/database/db_query_executor.hpp"
#include <chrono>
#include <ctime>
#include <boost/format.hpp>
#include <sys/types.h>
using namespace md::db;
using namespace md::smtp;
using namespace md::argument_parser;

void send_mail(const StringList &list, const std::string &smtp_host, unsigned smtp_port)
{
    SmtpServer mail;
    try {

        mail.set_security_type(USE_TLS);
        mail.init(list, smtp_host, smtp_port);
        if(mail.send_mail())
            mail.inc_send_success_count();

    }
    catch (SmtpException &e) {
        write_sys_log(e.get_error_message());
        std::cout << "Error: " << e.get_error_message().c_str() << ".\n";
        mail.inc_send_failed_count();
    }
}


int main(int argc, char const *argv[])
{

    try {
        auto parser = std::make_shared<ArgumentParser>();
        parser->start_parsing(argc, argv);
        std::string path_to_server_conf = parser->path_to_server_conf();
        std::string path_to_db_conf = parser->path_to_db_conf();
        SysErrorCode error_code;
        auto db_conf = read_config(path_to_db_conf, CONFIG_TYPE::DATABASE, error_code);

        auto server_conf = dynamic_cast<ServerConfig *>(read_config(path_to_server_conf, CONFIG_TYPE::SERVER,
                                                                    error_code).get());

        auto smtp_host = server_conf->get_domain();
        auto smtp_port = server_conf->get_port();
        auto server_count = server_conf->get_server_count();
        auto order_number = server_conf->get_order_number();
        auto process_count = server_conf->get_process_count();
        auto pg_backend = std::make_shared<PGBackend>();
        auto query_executor = std::make_shared<DbQueryExecutor>(db_conf);
        auto row_count = query_executor->get_row_count("core.emails");
        auto server_data_range = get_data_range(row_count, server_count, order_number);
        auto mail_data = query_executor->get_data4send_mail(server_data_range);
        /*for (const auto& data:mail_data) {
            send_mail(data, smtp_host, smtp_port);
        }*/
        auto process_row_count = abs(server_data_range.second - server_data_range.first);
        for (int process_idx = 0; process_idx < process_count; ++process_idx) {
            auto process_data_range = get_data_range(process_row_count, process_count, process_idx);
            auto mail_data2 = query_executor->get_data4send_mail(process_data_range);
            for (const auto& data:mail_data2) {
                send_mail(data, smtp_host, smtp_port);
            }
        }
        return 0;
    }
    catch (SmtpException &e) {
        write_sys_log(e.get_error_message());
    }

    catch (std::exception &e) {
        write_sys_log(e.what());
        std::cerr << e.what() << std::endl;
    }


}
