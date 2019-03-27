#pragma once

#include "../service/service.hpp"

namespace md
{
    namespace argument_parser
    {
        using namespace service;


        class ArgumentParser
        {
        public:
            ArgumentParser();

            ~ArgumentParser() = default;

            ArgumentParser(const ArgumentParser &) = delete;

            ArgumentParser(ArgumentParser &&) = default;

            ArgumentParser &operator=(const ArgumentParser &) = delete;

            ArgumentParser &operator=(ArgumentParser &&) = delete;

            bs::signal<void(const std::string &server_config, const std::string &db_config)> set_config_path;

            const std::string &path_to_db_conf() const;

            const std::string &path_to_server_conf() const;

        private:
            po::options_description m_general_options_description;

            std::string m_path_to_db_conf;

            std::string m_path_to_server_conf;

            void init_program_options();

            void error(const std::string &errorMessage);

        public:
            void start_parsing(int argc, const char **argv);


        };//class ArgumentParser

    }//namespace argument_parser

}