//
// Created by boa on 20.03.19.
//

#include "pg_connection.hpp"

namespace md
{
    namespace db
    {
        PGConnection::PGConnection()
        {
            m_connection.reset( PQsetdbLogin(m_host.c_str(), std::to_string(m_port).c_str(), nullptr, nullptr, m_database_name.c_str(), m_username.c_str(), m_password.c_str()), &PQfinish );

            if (PQstatus( m_connection.get() ) != CONNECTION_OK && PQsetnonblocking(m_connection.get(), 1) != 0 )
            {
                throw std::runtime_error( PQerrorMessage( m_connection.get() ) );
            }

        }


        std::shared_ptr<PGconn> PGConnection::connection() const
        {
            return m_connection;
        }




    }// namespace db
}// namespace md

