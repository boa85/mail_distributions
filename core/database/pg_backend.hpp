
#pragma once
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

            void setup_connection(
                    const std::string &host, const std::string &database_name, const std::string &username
                    , const std::string &password, int port);

            void setup_connection(ConfigPtr &ptr);
        private:
            void create_pool();

            std::mutex m_mutex;

            std::condition_variable m_condition;

            std::queue<std::shared_ptr<PGConnection>> m_pool;

            const int POOL_COUNT = 10;
        private:
            std::string m_host;
            int m_port = 5432;// default postgrtes port
            std::string m_database_name;
            std::string m_username;
            std::string m_password;

        };

        using PgbPtr = std::shared_ptr<PGBackend>;
    }// namespace db
}// namespace md

