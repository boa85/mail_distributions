#include "pg_backend.hpp"
#include <thread>

namespace md
{
    namespace db
    {
        PGBackend::PGBackend()
        {
            create_pool();
        }

        void PGBackend::create_pool()
        {
            std::lock_guard<std::mutex> locker(m_mutex);
            for (auto i = 0; i < POOL_COUNT; ++i) {
                if(auto conn = std::make_shared<PGConnection>()) {
                    conn->set_db_name(m_database_name);
                    conn->set_host(m_host);
                    conn->set_password(m_password);
                    conn->set_port(m_port);
                    conn->set_username(m_username);
                    if (conn->is_valid()) {
                        m_pool.push(conn);
                    } else {
                        throw std::runtime_error("can't setup connection");
                    }
                }
            }
        }

        std::shared_ptr<PGConnection> PGBackend::connection()
        {

            std::unique_lock<std::mutex> lock(m_mutex);

            while (m_pool.empty()) {
                m_condition.wait(lock);
            }

            auto front_connection = m_pool.front();
            m_pool.pop();
            return front_connection;
        }

        void PGBackend::free_connection(const std::shared_ptr<PGConnection>& connection)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pool.push(connection);
            lock.unlock();
            m_condition.notify_one();
        }

        void PGBackend::setup_connection(ConfigPtr &ptr)
        {
            auto db_conf = dynamic_cast<DbConfig *> (ptr.get());
            m_host = db_conf->m_hostname;
            m_database_name = db_conf->m_database_name;
            m_username = db_conf->m_username;
            m_password = db_conf->m_password;
            m_port = db_conf->m_port;
        }

        PGBackend::PGBackend(ConfigPtr &db_config)
        {
            setup_connection(db_config);
            create_pool();
        }

        void PGBackend::print()
        {
            std::cout << "host: " << m_host
                      << "\nport: " << m_port
                      << "\ndb name: " << m_database_name
                      << "\nusername: " << m_username
                      << "\npassword: " << m_password;
        }
    }// namespace db
}// namespace md


