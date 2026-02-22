#ifndef SORTED_SET_H
#define SORTED_SET_H
#include <unordered_map>
#include <set>
#include <string>
#include <optional>
#include <cstdint>
#include <vector>
namespace REDIS_NAMESPACE
{

    class SortedSet
    {
    private:
        std::unordered_map<std::string, double> hashMap{};
        std::set<std::pair<double, std::string>> sets{};
        size_t length{0};

    public:
        bool insert(std::string key, double score);
        bool remove(const std::string& member);
        std::optional<size_t> rank(const std::string& member) const;
        std::optional<double> score(const std::string& member) const;
        std::vector<std::string> range(int64_t start, int64_t stop) const;
        size_t size() const;
    };

}
#endif