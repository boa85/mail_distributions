
#pragma once

#include <vector>
#include <string>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/program_options.hpp>
#include <iostream>

using StringList = std::vector<std::string>;
namespace md
{
    namespace service
    {
        namespace bs=boost::signals2;
        namespace po=boost::program_options;
        namespace fs=boost::filesystem;



        /**
         * @brief isValidFile - checks the existence of the file and its status
         * @param filename - filename
         * @param errorCode - boost::system::error_code, return system error message
         * @return true, if a file exists and it is a regular file
         */
        bool isValidFile(const std::string &filename, boost::system::error_code &errorCode);

        bool is_valid_email(const std::string &email);

    }
}