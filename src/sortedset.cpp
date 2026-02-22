#include "sortedset.h"

namespace REDIS_NAMESPACE {

void SortedSet::insert(std::string key,double score)
{
    auto it = hashMap.find(key);
    
    if (it != hashMap.end()) {
        double oldScore = it->second;
        sets.erase({oldScore, key});
        hashMap.erase(it);
        length--;
    }
    
    hashMap[key] = score;
    sets.insert({score, key});
    length++;
}

size_t SortedSet::size() const
{
    return length;
}
}