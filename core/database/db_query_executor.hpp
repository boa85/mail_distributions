
#ifndef DB_QUERY_EXECUTOR_HPP
#define DB_QUERY_EXECUTOR_HPP

#include "db_tools.hpp"

namespace md
{
    using namespace service;
    namespace db
    {
        class DbQueryExecutor
        {
            PGBackendPtr m_pg_backend_ptr;
        public:
            explicit DbQueryExecutor(ConfigPtr &db_config);

            StringListArray get_data4send_mail(const DataRange &data_range);

            StringListArray get_data4send_mail(
                    const DataRange &data_range
                    , const std::string &login
                    , const std::string &password
                    , const std::string &sender_mail);

            int get_row_count(const std::string &table_name);

        private:
            void init(ConfigPtr &sharedPtr);
        };

    }
}





#endif //DB_QUERY_EXECUTOR_HPP
