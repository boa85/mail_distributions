
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

            int get_row_count(const std::string &table_name);

        private:
            void init(ConfigPtr &db_config);
        };

    }
}





#endif //DB_QUERY_EXECUTOR_HPP
