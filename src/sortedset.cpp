#include "sortedset.h"
#include <algorithm>
#include <vector>

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

bool SortedSet::remove(const std::string& member)
{
    auto it = hashMap.find(member);
    if (it == hashMap.end()) {
        return false;
    }
    
    double score = it->second;
    hashMap.erase(it);
    sets.erase({score, member});
    length--;
    return true;
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

std::optional<double> SortedSet::score(const std::string& member) const
{
    auto it = hashMap.find(member);
    if (it == hashMap.end()) {
        return std::nullopt;
    }
    return it->second;
}

    std::vector<std::string> SortedSet::range(int64_t start, int64_t stop) const
    {
        std::vector<std::string> result;
        size_t sz = sets.size();
        
        if (sz == 0) {
            return result;
        }
        
        if (start < 0) {
            start = static_cast<int64_t>(sz) + start;
        }
        if (stop < 0) {
            stop = static_cast<int64_t>(sz) + stop;
        }
        
        if (start < 0) {
            start = 0;
        }
        if (stop >= static_cast<int64_t>(sz)) {
            stop = static_cast<int64_t>(sz) - 1;
        }
        
        if (start > stop) {
            return result;
        }
        
        auto it = sets.begin();
        std::advance(it, start);
        
        for (int64_t i = start; i <= stop && it != sets.end(); ++i, ++it) {
            result.push_back(it->second);
        }
        
        return result;
    }

    std::vector<std::pair<std::string, double>> SortedSet::getAllWithScores() const
    {
        std::vector<std::pair<std::string, double>> result;
        for (const auto &entry : hashMap) {
            result.push_back({entry.first, entry.second});
        }
        return result;
    }
}