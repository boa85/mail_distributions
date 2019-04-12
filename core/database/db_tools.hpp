
#ifndef DB_TOOLS_HPP
#define DB_TOOLS_HPP

#include <memory>
#include "pg_backend.hpp"

namespace md
{
    using namespace service;
    namespace db
    {
        using PGBackendPtr = std::shared_ptr<PGBackend>;
    }
}

#endif //DB_TOOLS_HPP
