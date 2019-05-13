#include <thread>
#include <iostream>
#include <syslog.h>
#include <chrono>
#include <ctime>
#include <boost/format.hpp>
#include <sys/types.h>
#include <cpprest/json.h>
#include <cpprest/http_listener.h>
#include <cpprest/uri.h>
#include <cpprest/asyncrt_utils.h>


#include "core/smtp/smtp_server.hpp"
#include "core/database/pg_backend.hpp"
#include "tools/args_parser/argument_parser.hpp"
#include "core/database/db_tools.hpp"
#include "core/database/db_query_executor.hpp"
#include "core/rest/microsvc_controller.hpp"
#include "core/rest/foundation/include/usr_interrupt_handler.hpp"
#include "core/rest/foundation/include/runtime_utils.hpp"
using namespace utility;
using namespace web;
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
    catch (...) {
        std::cout << "Error: unknown error" << ".\n";
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
using namespace web;
//using namespace cfx;

int main(int argc, char const *argv[])
{

    InterruptHandler::hookSIGINT();

    MicroserviceController server;
    server.setEndpoint("http://host_auto_ip4:6502/v1/ivmero/api");


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


        auto p_server_conf = read_config(path_to_server_conf, CONFIG_TYPE::SERVER, error_code);
        if (!p_server_conf) {
            return -1;
        }
        auto server_conf = dynamic_cast<ServerConfig *>(p_server_conf.get());
        if (!server_conf) {
            return -1;
        }
        std::cerr << "server config will be read" << std::endl;
//        server_conf->print();

        std::string smtp_host = /*"smtp.yandex.ru"*/server_conf->get_domain();

        int smtp_port = /*587*/server_conf->get_port();
        int server_count = /*1*/server_conf->get_server_count();
        int order_number = /*1*/server_conf->get_order_number();
        auto process_count = /*1*/server_conf->get_process_count();
        auto row_count = global_query_executor->get_row_count("core.emails");

        auto server_data_range = get_data_range(row_count, server_count, order_number);
        auto mail_data = global_query_executor->get_data4send_mail(server_data_range);
        std::cout << "get data for send mail \n";
        auto process_row_count = abs(server_data_range.second - server_data_range.first);

        server.accept().wait();
        std::cout << "Modern C++ Microservice now listening for requests at: " << server.endpoint() << '\n';

        InterruptHandler::waitForUserInterrupt();

        server.shutdown().wait();

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
//            sleep(5);

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
