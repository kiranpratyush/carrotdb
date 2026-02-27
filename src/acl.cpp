#include "acl.h"

namespace ACL_NAMESPACE
{
    ACLManager::ACLManager()
    {
        defaultUser = std::make_shared<User>("default");
        defaultUser->nopass = true;
        defaultUser->is_enabled = true;
        defaultUser->flags.push_back("nopass");
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

    void ACLManager::setUser(const std::string &username, const std::string &hashedPassword, const std::vector<std::string> &flags)
    {
        auto it = users.find(username);
        std::shared_ptr<User> user;
        
        if (it != users.end())
        {
            user = it->second;
        }
        else
        {
            user = std::make_shared<User>(username);
            users[username] = user;
        }
        
        if (!hashedPassword.empty())
        {
            user->passwords.clear();
            user->passwords.push_back(hashedPassword);
            user->nopass = false;
        }
        
        user->flags = flags;
    }
}
