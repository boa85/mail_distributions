#include "pg_connection.hpp"

namespace md
{
    using namespace service;
    namespace db
    {
        /*PGConnection::PGConnection()
        {
            m_connection.reset(PQsetdbLogin(m_host.c_str(), std::to_string(m_port).c_str(), nullptr, nullptr,
                                            m_database_name.c_str(), m_username.c_str(), m_password.c_str()),
                               &PQfinish);

            if (PQstatus(m_connection.get()) != CONNECTION_OK && PQsetnonblocking(m_connection.get(), 1) != 0) {
                throw std::runtime_error(PQerrorMessage(m_connection.get()));
            }

        }*/


        std::shared_ptr<PGconn> PGConnection::connection() const
        {
            return m_connection;
        }

        PGConnection::PGConnection(const ConfigPtr &db_config)
        {
            auto db_conf = dynamic_cast<DbConfig *> (db_config.get());
            m_host = db_conf->m_hostname;
            m_database_name = db_conf->m_database_name;
            m_username = db_conf->m_username;
            m_password = db_conf->m_password;
            m_port = db_conf->m_port;

            m_connection.reset(PQsetdbLogin(m_host.c_str(), std::to_string(m_port).c_str(), nullptr, nullptr,
                                            m_database_name.c_str(), m_username.c_str(), m_password.c_str()),
                               &PQfinish);

            if (PQstatus(m_connection.get()) != CONNECTION_OK && PQsetnonblocking(m_connection.get(), 1) != 0) {
                throw std::runtime_error(PQerrorMessage(m_connection.get()));
            }

        }


    }// namespace db
}// namespace md

