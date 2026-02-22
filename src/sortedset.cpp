#include "sortedset.h"

namespace REDIS_NAMESPACE {

bool SortedSet::insert(std::string key,double score)
{
    auto it = hashMap.find(key);
    bool alreadyExists = false;
    if (it != hashMap.end()) {
        double oldScore = it->second;
        sets.erase({oldScore, key});
        hashMap.erase(it);
        length--;
        alreadyExists = true;
    }
    
    hashMap[key] = score;
    sets.insert({score, key});
    length++;
    return alreadyExists;
}

size_t SortedSet::size() const
{
    return length;
}
}