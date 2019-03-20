#include "pg_backend.hpp"
#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>
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
            std::lock_guard<std::mutex> locker_(m_mutex);

            for (auto i = 0; i < POOL_COUNT; ++i)
            {
                m_pool.emplace(std::make_shared<PGConnection>());
            }
        }

        std::shared_ptr<PGConnection> PGBackend::connection()
        {

            std::unique_lock<std::mutex> lock_(m_mutex);

            while (m_pool.empty())
            {
                m_condition.wait(lock_);
            }

            auto front_connection = m_pool.front();
            m_pool.pop();
            return front_connection;
        }


        void PGBackend::free_connection(std::shared_ptr<PGConnection> connection)
        {
            std::unique_lock<std::mutex> lock_(m_mutex);
            m_pool.push(connection);
            lock_.unlock();
            m_condition.notify_one();
        }

    }// namespace db
}// namespace md


