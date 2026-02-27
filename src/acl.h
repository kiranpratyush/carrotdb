#ifndef ACL_H
#define ACL_H
#include <memory>
#include <unordered_map>
#include "models/user.h"

namespace ACL_NAMESPACE
{
    class ACLManager
    {
    private:
        std::unordered_map<std::string, std::shared_ptr<User>> users;
        std::shared_ptr<User> defaultUser;

    public:
        ACLManager();
        std::shared_ptr<User> getDefaultUser();
        std::shared_ptr<User> getUser(const std::string &username);
        void addUser(std::shared_ptr<User> user);
        void setUser(const std::string &username, const std::string &hashedPassword, const std::vector<std::string> &flags);
    };
}
#endif