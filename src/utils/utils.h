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
}
