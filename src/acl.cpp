#include "acl.h"

namespace ACL_NAMESPACE
{
    ACLManager::ACLManager()
    {
        defaultUser = std::make_shared<User>("default");
        defaultUser->nopass = true;
        defaultUser->is_enabled = true;
        users["default"] = defaultUser;
    }

    std::shared_ptr<User> ACLManager::getDefaultUser()
    {
        return defaultUser;
    }

    std::shared_ptr<User> ACLManager::getUser(const std::string &username)
    {
        auto it = users.find(username);
        if (it != users.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void ACLManager::addUser(std::shared_ptr<User> user)
    {
        users[user->username] = user;
    }
}
