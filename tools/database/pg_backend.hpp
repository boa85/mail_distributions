
#ifndef MAIL_DISTRIBUTION_PG_BACKEND_HPP
#define MAIL_DISTRIBUTION_PG_BACKEND_HPP
#include <memory>
#include <mutex>
#include <string>
#include <queue>
#include <condition_variable>
#include <libpq-fe.h>
#include "pg_connection.hpp"

namespace md
{
    namespace db
    {
        class PGBackend
        {
        public:
            PGBackend();
            std::shared_ptr<PGConnection> connection();
            void free_connection(std::shared_ptr<PGConnection> connection);

        private:
            void create_pool();

            std::mutex m_mutex;
            std::condition_variable m_condition;
            std::queue<std::shared_ptr<PGConnection>> m_pool;

            const int POOL_COUNT = 10;


        };

    }// namespace db
}// namespace md



#endif //MAIL_DISTRIBUTION_PG_BACKEND_HPP
