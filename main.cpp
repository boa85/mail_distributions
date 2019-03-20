#include <thread>
#include <iostream>
#include "tools/database/pg_backend.hpp"

using namespace md::db;

void testConnection(const std::shared_ptr<PGBackend> &pgBackend)
{
    auto conn = pgBackend->connection();

    std::string demo = "SELECT max(id) FROM demo; ";
    PQsendQuery(conn->connection().get(), demo.c_str());

    while (auto res_ = PQgetResult(conn->connection().get())) {
        if (PQresultStatus(res_) == PGRES_TUPLES_OK && PQntuples(res_)) {
            auto ID = PQgetvalue(res_, 0, 0);
            std::cout << ID << std::endl;
        }

        if (PQresultStatus(res_) == PGRES_FATAL_ERROR) {
            std::cout << PQresultErrorMessage(res_) << std::endl;
        }

        PQclear(res_);
    }

    pgBackend->free_connection(conn);

}


int main(int argc, char const *argv[])
{
    try {
        auto pgbackend = std::make_shared<PGBackend>();


        std::vector<std::shared_ptr<std::thread>> vec;

        for (size_t i = 0; i < 50; ++i) {

            vec.push_back(std::make_shared<std::thread>(std::thread(testConnection, pgbackend)));
        }

        for (auto &i : vec) {
            i.get()->join();
        }


        return 0;
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
