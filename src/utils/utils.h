#pragma once
#include <string>

#define MAX_BUFFER_SIZE 4096

namespace REDIS_NAMESPACE
{
    inline bool convert_positive_string_to_number(std::string_view s, unsigned long long &number)
    {
        number = 0;
        for (auto x : s)
        {
            if (isdigit(x))
            {
                number = (number * 10) + (x - '0');
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    inline bool is_equal(std::string_view s1, std::string_view s2)
    {
        if (s1.size() != s2.size())
        {
            return false;
        }
        for (size_t i = 0; i < s1.size(); i++)
        {
            unsigned char x = static_cast<unsigned char>(s1[i]);
            unsigned char y = static_cast<unsigned char>(s2[i]);
            if (tolower(x) != tolower(y))
            {
                return false;
            }
        }
        return true;
    }

}
