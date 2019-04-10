#include <thread>
#include <iostream>
#include <syslog.h>
#include "core/smtp/smtp_server.hpp"
#include "core/database/pg_backend.hpp"
#include "tools/args_parser/argument_parser.hpp"
#include <chrono>
#include <ctime>
#include <boost/format.hpp>
using namespace md::db;
using namespace md::smtp;
using namespace md::argument_parser;

void send_mail(const StringList &list, const std::string &smtp_host, unsigned smtp_port)
{
    try {
        SmtpServer mail;
        mail.set_security_type(USE_TLS);
        mail.init(list, smtp_host, smtp_port);
        mail.send_mail();
    }
    catch (SmtpException &e) {
        write_sys_log(e.get_error_message());
        std::cout << "Error: " << e.get_error_message().c_str() << ".\n";
    }

}

void test_send_mail(
        const PGBackendPtr &pgBackend,
        const std::string &smtp_host,
        unsigned smtp_port,
        int server_count,
        int order_number,
        int process_count)
{
    auto connection = pgBackend->connection();

    std::string demo = "SELECT * FROM core.emails; ";

    std::string get_row_count = "SELECT count(*) FROM core.emails;";
    int row_count = 0;
    int begin = 0;
    int end = 0;
    int range = 0;
    PQsendQuery(connection->connection().get(), get_row_count.c_str());

    while (auto result = PQgetResult(connection->connection().get())) {
//        PGRES_COMMAND_OK
        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result)) {
            row_count = std::stoi(PQgetvalue(result, 0, 0));
        }
    }
    if (row_count != 0 && server_count > 0) {
        range = row_count / server_count;
        begin = order_number * range;
        end = begin + range;
        if (end > row_count) {
            end = row_count;
        }
    } else {
        throw std::runtime_error("invalid arguments");
    }
    std::string query = (boost::format("SELECT * FROM core.emails WHERE id BETWEEN %d AND %d ORDER BY\n"
                                       " id ASC;") % begin % end).str();
    PQsendQuery(connection->connection().get(), query.c_str());

    StringListArray query_result;

    while (auto result = PQgetResult(connection->connection().get())) {
        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result)) {
            auto pq_tuples_count = PQntuples(result);
            auto pq_fields_count = PQnfields(result);
            for (auto i = 0; i < pq_tuples_count; ++i) {
                StringList list;
                for (auto j = 0; j < pq_fields_count; ++j) {
                    auto item = PQgetvalue(result, i, j);
                    list.emplace_back(item);
                }
                send_mail(list, smtp_host, smtp_port);
                query_result.emplace_back(list);
            }
        }

        if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
            write_sys_log(PQresultErrorMessage(result));
            std::cout << PQresultErrorMessage(result) << std::endl;
        }

        PQclear(result);
    }

    for (const auto &list:query_result) {
        std::cout << list << std::endl;
    }

    pgBackend->free_connection(connection);

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
        auto pg_backend = std::make_shared<PGBackend>();
        auto server_conf = dynamic_cast<ServerConfig *>(read_config(path_to_server_conf, CONFIG_TYPE::SERVER,
                                                                    error_code).get());

        auto smtp_host = server_conf->get_domain();
        auto smtp_port = server_conf->get_port();
        auto server_count = server_conf->get_server_count();
        auto order_number = server_conf->get_order_number();
        auto process_count = server_conf->get_process_count();
        pg_backend->setup_connection(db_conf);
        std::vector<std::shared_ptr<std::thread>> vec;
        for (size_t i = 0; i < 1; ++i) {
            vec.push_back(std::make_shared<std::thread>(
                    std::thread(test_send_mail, pg_backend, smtp_host, smtp_port, server_count, order_number,
                                process_count)));
        }

        for (auto &i : vec) {
            i.get()->join();
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
