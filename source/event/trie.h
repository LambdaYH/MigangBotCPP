#ifndef MIGANGBOT_EVENT_HANDLER_TRIE_H_
#define MIGANGBOT_EVENT_HANDLER_TRIE_H_

#include <algorithm>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <string_view>
#include <nlohmann/json.hpp>
#include "event/event.h"
#include "bot/onebot_11/api_bot.h"
#include "event/search_result.h"
#include "event/types.h"

namespace white
{

class Trie
{
public:
    Trie();
    ~Trie();

public:
    template<typename F>
    bool Insert(const std::string &key, F &&func);

    template<typename F>
    bool InsertFromBack(const std::string &key, F &&func);

    const SearchResult Search(const std::string &key) const;

    const SearchResult SearchFromBack(const std::string &key) const;

private:
    struct TrieNode
    {
        std::unordered_map<char, std::shared_ptr<TrieNode>> childs;
        plugin_func func;
    };

private:
    std::shared_ptr<TrieNode> root_;
    plugin_func no_func_here_;
};

inline Trie::Trie() :
root_(std::make_shared<TrieNode>()),
no_func_here_()
{

}

inline Trie::~Trie()
{

}

template<typename F>
inline bool Trie::Insert(const std::string &key, F &&func)
{
    auto cur_node = root_;
    for(auto ch : key)
    {
        ch = std::tolower(ch);
        if(!cur_node->childs.count(ch))
            cur_node->childs.emplace(ch, std::make_shared<TrieNode>());
        cur_node = cur_node->childs[ch];
    }
    if(cur_node->func)
        return false; 
    cur_node->func = std::forward<F>(func);
    return true;
}

template<typename F>
inline bool Trie::InsertFromBack(const std::string &key, F &&func)
{
    auto cur_node = root_;
    for(auto it = key.rbegin(); it != key.rend(); ++it)
    {
        auto ch = std::tolower(*it);
        if(!cur_node->childs.count(ch))
            cur_node->childs.emplace(ch, std::make_shared<TrieNode>());
        cur_node = cur_node->childs[ch];
    }
    if(cur_node->func)
        return false; 
    cur_node->func = std::forward<F>(func);
    return true;
}

inline const SearchResult Trie::Search(const std::string &key) const
{
    auto cur_node = root_;
    short command_size{ 0 };
    for(auto ch : key)
    {
        ch = std::tolower(ch);
        if(ch == ' ')
            break;
        ++command_size;
        if (!cur_node->childs.count(ch))
            return {no_func_here_, 0};
        cur_node = cur_node->childs[ch];
        if(cur_node->func) 
            return {cur_node->func, command_size};
    }
    if(cur_node->func)
        return {cur_node->func, command_size};
    return {no_func_here_, 0};
}

inline const SearchResult Trie::SearchFromBack(const std::string &key) const
{
    auto cur_node = root_;
    short command_size{ 0 };
    for(auto it = key.rbegin(); it != key.rend(); ++it)
    {
        auto ch = std::tolower(*it);
        if(ch == ' ')
            break;
        --command_size;
        if (!cur_node->childs.count(ch))
            return {no_func_here_, 0};
        cur_node = cur_node->childs[ch];
        if(cur_node->func) 
            return {cur_node->func, command_size};
    }
    if (cur_node->func) 
        return {cur_node->func, command_size};
    return {no_func_here_, 0};
}

} // namespace white

#endif