
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

            explicit PGBackend(ConfigPtr &db_config);

            std::shared_ptr<PGConnection> connection();

            void free_connection(const std::shared_ptr<PGConnection>& connection);

            void setup_connection(ConfigPtr &ptr);
        private:
            void create_pool(const ConfigPtr& db_config);

            void print();

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

    }// namespace db
}// namespace md

