#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

class RadixTreeNode
{
public:
    std::string data{};
    std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> children{};
    bool isDataPresent{false};
};

class RadixTree
{
    std::unique_ptr<RadixTreeNode> root{};

    size_t matchedLength(std::string_view new_key, std::string_view existing_key);

public:
    void insert(std::string key, std::string_view value);
    std::string search(std::string key);

private:
    void insertImpl(RadixTreeNode *node, std::string key, std::string_view value);
};