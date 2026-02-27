#ifndef USER_H
#define USER_H
#include <string>
#include <vector>


namespace ACL_NAMESPACE
{
    struct User
    {
        std::string username;
        std::vector<std::string> passwords;
        std::vector<std::string> flags;
        bool is_enabled;
        bool nopass;

        User() : username("default"), is_enabled(true), nopass(true) {}

        explicit User(const std::string &name) : username(name), is_enabled(true), nopass(false) {}
    };
}
#endif