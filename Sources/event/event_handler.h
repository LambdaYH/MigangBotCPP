#ifndef MIGANGBOTCPP_EVENT_HANDLER_EVENT_HANDLER_H_
#define MIGANGBOTCPP_EVENT_HANDLER_EVENT_HANDLER_H_

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "event/trie.h"
#include "event/event_filter.h"
#include "event/event.h"
#include "pool/thread_pool.h"
#include "logger/logger.h"
#include "bot/onebot_11/api_bot.h"
#include "utility.h"
#include "permission/permission.h"
#include "event/types.h"

#include <iostream>

namespace white
{

const std::unordered_map<int, int> perm_to_loc{
    {permission::BLACK,         0},
    {permission::NORMAL,        1},
    {permission::PRIVATE,       2},
    {permission::GROUP_MEMBER,  3},
    {permission::GROUP_ADMIN,   4},
    {permission::GROUP_OWNER,   5},
    {permission::WHITE_LIST,    6},
    {permission::SUPERUSER,     7}
};

class EventHandler
{
public:
    static EventHandler &GetInstance()
    {
        static EventHandler event_handler;
        return event_handler;
    }

    void Init(std::size_t thread_num)
    {
        pool_ = std::make_unique<ThreadPool>(thread_num);
    }

    void InitFilter(std::unique_ptr<EventFilter> &&filter)
    {
        filter_ = std::move(filter);
    }

public:
    template<typename F>
    bool RegisterCommand(const int command_type, const std::string &command, F &&func, int permission = permission::NORMAL, bool only_to_me = false);

    template<typename F>
    bool RegisterNotice(const std::string &notice_type, const std::string &sub_type, F &&func);

    template<typename F>
    bool RegisterRequest(const std::string &request_type, const std::string &sub_type, F &&func);

    bool Handle(Event &event, onebot11::ApiBot &bot) const;

    template<typename F>
    void AddTask(F &&func);

private:
    EventHandler() :
        command_fullmatch_each_perm_(8),
        command_prefix_each_perm_(8),
        command_suffix_each_perm_(8),
        command_keyword_each_perm_(8),
        command_fullmatch_each_perm_to_me_(8),
        command_prefix_each_perm_to_me_(8),
        command_suffix_each_perm_to_me_(8),
        command_keyword_each_perm_to_me_(8),
        all_msg_handler_each_perm_(8)
    {}
    ~EventHandler() {}

    EventHandler(const EventHandler &) = delete;
    EventHandler &operator=(const EventHandler &) = delete;
    EventHandler(const EventHandler &&) = delete;
    EventHandler &operator=(const EventHandler &&) = delete;

private:
    const plugin_func &MatchedHandler(Event &event) const;
    const SearchResult MatchHelper(int permission, const std::string &msg, bool only_to_me) const;
    const std::vector<plugin_func> FreeHandler(const Event &event) const;

private:
    std::vector<std::unordered_map<std::string, plugin_func>> command_fullmatch_each_perm_;
    std::vector<Trie> command_prefix_each_perm_;
    std::vector<Trie> command_suffix_each_perm_;
    std::vector<std::unordered_map<std::string, plugin_func>> command_keyword_each_perm_;

    std::vector<std::unordered_map<std::string, plugin_func>> command_fullmatch_each_perm_to_me_;
    std::vector<Trie> command_prefix_each_perm_to_me_;
    std::vector<Trie> command_suffix_each_perm_to_me_;
    std::vector<std::unordered_map<std::string, plugin_func>> command_keyword_each_perm_to_me_;

    std::vector<std::vector<plugin_func>> all_msg_handler_each_perm_;

    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<plugin_func>>> notice_handler_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<plugin_func>>> request_handler_;
    plugin_func no_func_avaliable_;

    std::unique_ptr<EventFilter> filter_;
    std::unique_ptr<ThreadPool> pool_;
};

template<typename F>
inline bool EventHandler::RegisterCommand(const int command_type, const std::string &command, F &&func, int permission, bool only_to_me)
{
    if(!perm_to_loc.count(permission))
        return false;
    switch(command_type)
    {
        case FULLMATCH:
            if(only_to_me)
                return command_fullmatch_each_perm_to_me_[perm_to_loc.at(permission)].emplace(std::move(command), std::forward<F>(func)).second;
            return command_fullmatch_each_perm_[perm_to_loc.at(permission)].emplace(std::move(command), std::forward<F>(func)).second;
            break;
        case PREFIX:
            if(only_to_me)
                return command_prefix_each_perm_to_me_[perm_to_loc.at(permission)].Insert(std::move(command), std::forward<F>(func));
            return command_prefix_each_perm_[perm_to_loc.at(permission)].Insert(std::move(command), std::forward<F>(func));
            break;
        case SUFFIX:
        {
            if(only_to_me)
                return command_suffix_each_perm_to_me_[perm_to_loc.at(permission)].InsertFromBack(std::move(command), std::forward<F>(func));
            return command_suffix_each_perm_[perm_to_loc.at(permission)].InsertFromBack(std::move(command), std::forward<F>(func));
            break;
        }
        case KEYWORD:
            if(only_to_me)
                return command_keyword_each_perm_to_me_[perm_to_loc.at(permission)].emplace(std::move(command), std::forward<F>(func)).second;
            return command_fullmatch_each_perm_[perm_to_loc.at(permission)].emplace(std::move(command), std::forward<F>(func)).second;
            break;
        case ALLMSG:
            all_msg_handler_each_perm_[perm_to_loc.at(permission)].push_back(std::forward<F>(func));
            return true;
            break;
        default:
            return command_fullmatch_each_perm_[perm_to_loc.at(permission)].emplace(std::move(command), std::forward<F>(func)).second;
    }
    return true;
}

template<typename F>
inline bool EventHandler::RegisterNotice(const std::string &notice_type, const std::string &sub_type, F &&func)
{
    notice_handler_[notice_type][sub_type].push_back(std::forward<F>(func));
    return true;
}

template<typename F>
inline bool EventHandler::RegisterRequest(const std::string &request_type, const std::string &sub_type, F &&func)
{
    request_handler_[request_type][sub_type].push_back(std::forward<F>(func));
    return true;
}

inline bool EventHandler::Handle(Event &event, onebot11::ApiBot &bot) const
{
    if(filter_ && !filter_->operator()(event))
        return false;
    if(event.contains("post_type"))
    {
        auto post_type = event["post_type"].get<std::string>();
        switch(post_type[3])
        {
            // message
            case 's':
            {
                if(event["message_type"].get<std::string>()[0] == 'p')
                {
                    if(event["sub_type"].get<std::string>()[0] == 'f')
                        LOG_INFO("Bot[{}]收到来自好友[{}({})]的消息: {}", 
                            event["self_id"].get<QId>(), 
                            event["sender"].value("nickname", ""),
                            event["sender"].value("user_id", 0),
                            event.value("message", "Unknown message")
                        );
                }else
                {
                    if(event["anonymous"] != nullptr)
                    {
                        LOG_INFO("Bot[{}]收到来自群[{}]的匿名消息: {}", 
                            event["self_id"].get<QId>(), 
                            event["group_id"].get<GId>(),
                            event.value("message", "Unknown message")
                        );
                    }else
                    {
                        LOG_INFO("Bot[{}]收到来自群[{}]成员[{}({})]的消息: {}", 
                            event["self_id"].get<QId>(), 
                            event["group_id"].get<GId>(),
                            event["sender"].value("nickname", ""),
                            event["sender"].value("user_id", 0),
                            event.value("message", "Unknown message")
                        );
                    }   
                }
                auto func = MatchedHandler(event);
                if(func)
                    pool_->AddTask(std::bind(func, event, std::ref(bot))); // 原对象会消失，event必须拷贝
                else
                {
                    auto &funcs = FreeHandler(event);
                    for(auto &func : funcs)
                        pool_->AddTask(std::bind(func, event, std::ref(bot)));
                }  
            }
            break;
            // notice
            case 'i':
            {
                auto notice_type = event.value("notice_type", "");
                auto sub_type = event.value("sub_type", "");
                LOG_INFO("Bot[{}] 收到一个通知事件: {}.{}", event["self_id"].get<QId>(), notice_type, sub_type);
                if(notice_handler_.count(notice_type) && notice_handler_.at(notice_type).count(sub_type))
                {
                    for(auto &func : notice_handler_.at(notice_type).at(sub_type))
                        pool_->AddTask(std::bind(func, event, std::ref(bot)));
                }
            }
            break;
            // request
            case 'u':
            {
                auto request_type = event.value("request_type", "");
                auto sub_type = event.value("sub_type", "");
                LOG_INFO("Bot[{}] 收到一个请求事件: {}.{}", event["self_id"].get<QId>(), request_type, sub_type);
                if(request_handler_.count(request_type) && request_handler_.at(request_type).count(sub_type))
                {
                    for(auto &func : request_handler_.at(request_type).at(sub_type))
                        pool_->AddTask(std::bind(func, event, std::ref(bot)));
                }
            }
            break;
            // meta_event
            case 'a':
            {
                auto meta_event_type = event["meta_event_type"].get<std::string>();
                if(meta_event_type[0] == 'l')
                {
                    auto sub_type = event["sub_type"].get<std::string>();
                    switch(sub_type[0])
                    {
                        case 'e':
                        case 'c':
                            LOG_INFO("Bot[{}]已成功建立连接", event["self_id"].get<QId>());
                            break;
                        case 'd':
                            LOG_INFO("Bot[{}]已断开连接", event["self_id"].get<QId>());
                            break;
                    }
                }else
                {
                    LOG_DEBUG("Bot[{}]与[{}]收到一次心跳连接", event["self_id"].get<QId>(), event["time"].get<int64_t>());
                }
            }
            break;
            default:
                break;
        }
    }
    return true;
}

inline const plugin_func &EventHandler::MatchedHandler(Event &event) const
{
    if(event.is_null())
        return no_func_avaliable_;
    auto msg = event["message"].get<std::string_view>();
    if(msg.empty())
        return no_func_avaliable_;
    bool only_to_me = false;
    if(msg.starts_with("[CQ:at"))
    {
        auto at_id_start = msg.find_first_of('=') + 1;
        if(at_id_start != std::string_view::npos)
        {
            auto at_id_end = msg.find_first_of(']');
            if(at_id_end != std::string_view::npos)
            {
                auto at_id = msg.substr(at_id_start, at_id_end - at_id_start);
                auto self_id = std::to_string(event["self_id"].get<QId>());
                if(at_id == self_id)
                {
                    only_to_me = true;
                    msg = msg.substr(msg.find_first_of(']') + 1);
                    msg = msg.substr(msg.find_first_not_of(' '));
                    event["__to_me__"] = true;
                }
            }
        }
    }
    auto msg_str = std::string(msg);
    auto perm = permission::GetUserPermission(event);
    switch(perm)
    {
        case permission::SUPERUSER:
        {
            auto func_ret = MatchHelper(permission::SUPERUSER, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }
        case permission::WHITE_LIST:
        {
            auto func_ret = MatchHelper(permission::WHITE_LIST, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }       
        case permission::GROUP_OWNER:
        {
            auto func_ret = MatchHelper(permission::GROUP_OWNER, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }            
        case permission::GROUP_ADMIN:
        {
            auto func_ret = MatchHelper(permission::GROUP_ADMIN, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        } 
        case permission::GROUP_MEMBER:
        {
            auto func_ret = MatchHelper(permission::GROUP_MEMBER, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }
        case permission::PRIVATE:
        {
            auto func_ret = MatchHelper(permission::PRIVATE, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }
        case permission::NORMAL:
        {
            auto func_ret = MatchHelper(permission::NORMAL, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }   
        case permission::BLACK:
        {
            auto func_ret = MatchHelper(permission::BLACK, msg_str, only_to_me);
            if (func_ret.func)
            {
                event["__command_size__"] = func_ret.command_size;
                return func_ret.func;
            }
        }
        default:
            return no_func_avaliable_;
    }   
    return no_func_avaliable_;
}

inline const SearchResult EventHandler::MatchHelper(int permission, const std::string &msg, bool only_to_me) const
{
    if(only_to_me)
    {
        if (command_fullmatch_each_perm_to_me_[perm_to_loc.at(permission)].count(msg))
            return {command_fullmatch_each_perm_to_me_[perm_to_loc.at(permission)].at(msg), 0};
        auto &func_prefix_result = command_prefix_each_perm_to_me_[perm_to_loc.at(permission)].Search(msg);
        if(func_prefix_result.func)
            return func_prefix_result;
        auto &func_suffix_result = command_suffix_each_perm_to_me_[perm_to_loc.at(permission)].SearchFromBack(msg);
        if(func_suffix_result.func)
            return func_suffix_result;
    }else
    {
        if (command_fullmatch_each_perm_[perm_to_loc.at(permission)].count(msg))
            return {command_fullmatch_each_perm_[perm_to_loc.at(permission)].at(msg), 0};
        auto &func_prefix_result = command_prefix_each_perm_[perm_to_loc.at(permission)].Search(msg);
        if(func_prefix_result.func)
            return func_prefix_result;
        auto &func_suffix_result = command_suffix_each_perm_[perm_to_loc.at(permission)].SearchFromBack(msg);
        if(func_suffix_result.func)
            return func_suffix_result;
    }
    return {no_func_avaliable_, 0};
}

inline const std::vector<plugin_func> EventHandler::FreeHandler(const Event &event) const
{
    std::vector<plugin_func> ret;
    auto perm = permission::GetUserPermission(event);
    switch(perm)
    {
        case permission::SUPERUSER:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::SUPERUSER)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::SUPERUSER)].end());
        case permission::WHITE_LIST:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::WHITE_LIST)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::WHITE_LIST)].end());
        case permission::GROUP_OWNER:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::GROUP_OWNER)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::GROUP_OWNER)].end());              
        case permission::GROUP_ADMIN:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::GROUP_ADMIN)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::GROUP_ADMIN)].end()); 
        case permission::GROUP_MEMBER:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::GROUP_MEMBER)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::GROUP_MEMBER)].end());   
        case permission::PRIVATE:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::PRIVATE)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::PRIVATE)].end());
        case permission::NORMAL:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::NORMAL)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::NORMAL)].end());
        case permission::BLACK:
            ret.insert(ret.end(), all_msg_handler_each_perm_[perm_to_loc.at(permission::BLACK)].begin(), all_msg_handler_each_perm_[perm_to_loc.at(permission::BLACK)].end()); 
        default:
            break;
    } 
    return ret;
}

template<typename F>
inline void EventHandler::AddTask(F &&func)
{
    pool_->AddTask(std::forward<F>(func));
}

} // namespace white

#endif