cmake_minimum_required(VERSION 3.13)
project(mail_distribution)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O0 -pedantic-errors -Wextra -Wall -Winit-self -Wold-style-cast -Woverloaded-virtual -Wuninitialized -Wmissing-declarations -Wno-ignored-qualifiers -Wno-unused-parameter")
find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
else ()
    message(FATAL_ERROR "Could not find openssl!")
endif ()

find_package(Boost 1.58.0 COMPONENTS system filesystem program_options signals date_time thread regex serialization)
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find boost!")
endif ()
find_package(PostgreSQL REQUIRED)
include_directories(${BOOST_INCLUDE_DIRS}  ${PostgreSQL_INCLUDE_DIRS} )
SET(CMAKE_CXX_FLAGS -pthread)
set(SOURCE_FILES
        main.cpp
        core/database/pg_backend.cpp
        core/database/pg_backend.hpp
        core/database/pg_connection.cpp
        core/database/pg_connection.hpp
        core/smtp/smtp_server.cpp
        core/smtp/smtp_server.hpp
        tools/service/base_64.cpp
        tools/service/base_64.hpp
        tools/service/service.cpp
        tools/service/service.hpp
        tools/args_parser/argument_parser.cpp
        tools/args_parser/argument_parser.hpp
        core/smtp/md_5.cpp
        core/smtp/md_5.hpp core/smtp/smtp_exception.cpp core/smtp/smtp_exception.hpp core/smtp/smtp_common.cpp core/smtp/smtp_common.hpp)
add_executable(mail_distribution ${SOURCE_FILES} main.cpp)
target_link_libraries(mail_distribution
        ${Boost_LIBRARIES}
        ${PostgreSQL_LIBRARIES}
        ${OPENSSL_LIBRARIES})