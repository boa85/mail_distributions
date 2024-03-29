
#include <boost/format.hpp>
#include "db_query_executor.hpp"
#include "../../tools/service/service.hpp"
namespace md
{
    using namespace service;
    namespace db
    {

        DbQueryExecutor::DbQueryExecutor(ConfigPtr &db_config)
        {
            init(db_config);
        }

        void DbQueryExecutor::init(ConfigPtr &db_config)
        {
            if (!m_pg_backend_ptr) {
                m_pg_backend_ptr = std::make_shared<PGBackend>(db_config);
            }
            m_pg_backend_ptr->setup_connection(db_config);
        }

        StringListArray DbQueryExecutor::get_data4send_mail(const DataRange &data_range)
        {
            if(auto connection = m_pg_backend_ptr->connection()) {
                std::string query = (boost::format("SELECT * FROM core.emails WHERE id BETWEEN %d AND %d ORDER BY\n"
                                                   " id ASC;") % data_range.first % data_range.second).str();
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
                            query_result.emplace_back(list);
                        }
                    }

                    if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
                        write_sys_log(PQresultErrorMessage(result));
                        std::cout << PQresultErrorMessage(result) << std::endl;
                    }
                    PQclear(result);
                }
                m_pg_backend_ptr->free_connection(connection);
                return query_result;
            }
            return StringListArray();
        }

        int DbQueryExecutor::get_row_count(const std::string &table_name)
        {
            if(auto connection = m_pg_backend_ptr->connection()) {
                std::string get_row_count = (boost::format("SELECT count(*) FROM %s;") % table_name).str();
                int row_count = 0;

                PQsendQuery(connection->connection().get(), get_row_count.c_str());

                while (auto result = PQgetResult(connection->connection().get())) {
                    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result)) {
                        row_count = std::stoi(PQgetvalue(result, 0, 0));
                        break;
                    }
                    if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
                        write_sys_log(PQresultErrorMessage(result));
                        std::cout << PQresultErrorMessage(result) << std::endl;
                    }
                    PQclear(result);
                }

                m_pg_backend_ptr->free_connection(connection);
                return row_count;
            }
            return 0;
        }

        StringListArray DbQueryExecutor::get_data4send_mail(const DataRange &data_range, const std::string &login
                                                            , const std::string &password
                                                            , const std::string &sender_mail)
        {

            if(auto connection = m_pg_backend_ptr->connection()) {
                std::string query = (boost::format("SELECT * FROM core.emails WHERE id BETWEEN %d AND %d ORDER BY\n"
                                                   " id ASC;") % data_range.first % data_range.second).str();
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
                            query_result.emplace_back(list);
                        }
                    }

                    if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
                        write_sys_log(PQresultErrorMessage(result));
                        std::cout << PQresultErrorMessage(result) << std::endl;
                    }
                    PQclear(result);
                }
                m_pg_backend_ptr->free_connection(connection);
                return query_result;
            }

            return StringListArray();
        }
    }
}
