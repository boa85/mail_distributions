#pragma  once
#include <memory>
#include <mutex>
#include <libpq-fe.h>

namespace md
{
    namespace db
    {
        class PGConnection
        {
        public:
            PGConnection();

            std::shared_ptr<PGconn> connection() const;

        private:
            void establish_connection();

            std::string m_host = "localhost";
            int m_port = 5432;
            std::string m_database_name = "demo";
            std::string m_username = "postgres";
            std::string m_password = "postgres";
            std::shared_ptr<PGconn> m_connection;

        };


    }// namespace db
}// namespace md
