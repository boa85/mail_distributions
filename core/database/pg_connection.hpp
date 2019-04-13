#pragma  once
#include <memory>
#include <mutex>
#include <libpq-fe.h>
#include "../../tools/service/service.hpp"

namespace md
{
    using namespace service;
    namespace db
    {

        class PGConnection
        {
        public:
//            PGConnection();

            explicit PGConnection(const ConfigPtr &db_config);

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
            /*username=maildistributionuser
password=8dk76SF#Y
db_name=maildistributiondb
hostname=78.107.249.53
port=5432
*/
            std::string m_host/* = "78.107.249.53"*/;
            int m_port = 5432;
            std::string m_database_name/* = "maildistributiondb"*/;
            std::string m_username/* = "maildistributionuser"*/;
            std::string m_password/* = "8dk76SF#Y"*/;
            std::shared_ptr<PGconn> m_connection;

        };


    }// namespace db
}// namespace md
