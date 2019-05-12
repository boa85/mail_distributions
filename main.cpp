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
std::shared_ptr<DbQueryExecutor> global_query_executor;
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
void do_child(DataRange range, std::string &smtp_host, unsigned smtp_port)
{
    auto mail_data = global_query_executor->get_data4send_mail(range);
    std::cout << mail_data;
    for (const auto &data:mail_data) {

        send_mail(data, smtp_host, smtp_port);
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
        write_sys_log("start read config", LOG_DEBUG);
        auto db_conf = read_config(path_to_db_conf, CONFIG_TYPE::DATABASE, error_code);
        write_sys_log("db config read success", LOG_DEBUG);


        global_query_executor = std::make_shared<DbQueryExecutor>(db_conf);
        std::cerr << "db_query executor created" << std::endl;

        auto row_count = global_query_executor->get_row_count("core.emails");
        auto server_conf = dynamic_cast<ServerConfig *>(read_config(path_to_server_conf, CONFIG_TYPE::SERVER,
                                                                 error_code).get());
        std::cerr << "db_query executor created" << std::endl;

        auto smtp_host = server_conf->get_domain();
        server_conf->print();
        auto smtp_port = server_conf->get_port();
        auto server_count = server_conf->get_server_count();
        auto order_number = server_conf->get_order_number();
        auto process_count = server_conf->get_process_count();
        auto server_data_range = get_data_range(row_count, server_count, order_number);
        auto mail_data = global_query_executor->get_data4send_mail(server_data_range);
        write_sys_log("get data for send mail ", LOG_DEBUG);
        auto process_row_count = abs(server_data_range.second - server_data_range.first);

        for (int process_idx = 1; process_idx <= process_count; ++process_idx) {
            auto process_data_range = get_data_range(process_row_count + 1, process_count, process_idx);
            pid_t pid = fork();
            if (pid < 0) {
                write_sys_log("can't to fork", LOG_DEBUG);
                continue;
            }
            if (pid == 0) {
                do_child(process_data_range, smtp_host, smtp_port);
            } else if (process_idx == 1) {
                do_child(process_data_range, smtp_host, smtp_port);
            }
            sleep(5);

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
