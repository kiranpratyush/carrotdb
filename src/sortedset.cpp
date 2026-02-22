#include "sortedset.h"
#include <algorithm>

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

std::optional<size_t> SortedSet::rank(const std::string& member) const
{
    auto it = hashMap.find(member);
    if (it == hashMap.end()) {
        return std::nullopt;
    }
    
    double score = it->second;
    auto setIt = sets.find({score, member});
    if (setIt == sets.end()) {
        return std::nullopt;
    }
    
    return static_cast<size_t>(std::distance(sets.begin(), setIt));
}

size_t SortedSet::size() const
{
    return length;
}
}