
#include "argument_parser.hpp"

namespace md
{
    namespace argument_parser
    {

        ArgumentParser::ArgumentParser() : m_general_options_description("program options")
        {
            init_program_options();
        }//ArgumentParser

        void ArgumentParser::init_program_options()
        {

            m_general_options_description.add_options()
                    ("help,h",
                     "\nmail distribution v 0.1\n"
                     "author: boa\n"
                     "program options: \n "
                     "path  to database config \n"
                     "path  to server config \n"
                     "e.g ./mail_distribution -d db.conf -s server.conf\n"
                     "start SMTP server ")
                    ("db,d", po::value<std::string>(&m_path_to_db_conf)->required(),
                     "path to file with database options")
                    ("srv,s", po::value<std::string>(&m_path_to_server_conf)->required(),
                     "path to file with server options");

        }//init_program_options


        void ArgumentParser::error(const std::string &errorMessage)
        {
            std::cout << errorMessage << std::endl << "See help " << std::endl << m_general_options_description
                      << std::endl;
            throw std::logic_error(errorMessage);
        }//error

        void ArgumentParser::start_parsing(int argc, const char **argv)
        {
            /** Concrete variables map which store variables in real map.
             * This class is derived from std::map<std::string, variable_value>,
             * so you can use all map operators to examine its content.
            */
            po::variables_map vm;//
            po::parsed_options parsed =//magic
                    po::command_line_parser(argc, argv).options(
                            m_general_options_description).allow_unregistered().run();

            po::store(parsed, vm);//more magic
            po::notify(vm);//even more magic

            if (vm.count("help") != 0u) {//if found a help flag
                std::cout << m_general_options_description;//show help
                return;
            }
            if (!m_path_to_server_conf.empty() && !m_path_to_db_conf.empty()) {
                set_config_path(m_path_to_server_conf, m_path_to_db_conf);
            }

        }

        const std::string &ArgumentParser::path_to_db_conf() const
        {
            return m_path_to_db_conf;
        }

        const std::string &ArgumentParser::path_to_server_conf() const
        {
            return m_path_to_server_conf;
        }
// start_parsing

    }// argument_parser

}