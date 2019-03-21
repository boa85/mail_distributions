//
// Created by boa on 20.03.19.
//

#ifndef MAIL_DISTRIBUTION_BASE_64_HPP
#define MAIL_DISTRIBUTION_BASE_64_HPP
#include <string>
namespace md
{
    namespace smtp
    {
        std::string base64_encode(unsigned char const* , unsigned int len);
        std::string base64_decode(std::string const& s);
    }
}



#endif //MAIL_DISTRIBUTION_BASE_64_HPP
