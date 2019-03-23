
#include <regex>
#include "service.hpp"

bool md::service::is_valid_email(const std::string &email)
{
    const std::regex pattern(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");

    return std::regex_match(email, pattern);
}

bool md::service::isValidFile(const std::string &filename, boost::system::error_code &errorCode)
{
    return (fs::exists(filename, errorCode) && fs::is_regular(filename, errorCode));
}
