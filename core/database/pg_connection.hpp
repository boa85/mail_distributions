#pragma  once
#include <memory>
#include <mutex>
#include <libpq-fe.h>
#include "../../tools/service/service.hpp"

namespace md
{
    namespace db
    {
        using namespace service;
        class PGConnection
        {
        public:
            PGConnection();

            std::shared_ptr<PGconn> connection() const;

            const std::string &host() const
            {
                return m_host;
            }

            void set_host(const std::string &host)
            {
                m_host = host;
            }


            void set_port(int port)
            {
                m_port = port;
            }


            void set_db_name(const std::string &database_name)
            {
                m_database_name = database_name;
            }


            void set_username(const std::string &username)
            {
                m_username = username;
            }


            void set_password(const std::string &password)
            {
                m_password = password;
            }

            bool is_valid() const
            {
                return true;
            }
        private:
            void establish_connection();


        private:
            std::string m_host = "localhost";
            int m_port = 5432;
            std::string m_database_name = "mail_distribution";
            std::string m_username = "postgres";
            std::string m_password = "nfy.if";
            std::shared_ptr<PGconn> m_connection;

        };


    }// namespace db
}// namespace md
