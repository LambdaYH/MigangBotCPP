#pragma once

#include <queue>
#include <string>
#include <vector>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "modules/module/weibo/utils.h"

using Json = nlohmann::json;

namespace white {
namespace module {
namespace weibo {
class WeiboSpider {
 public:
  WeiboSpider(std::vector<std::string> &&filter_words, bool filter,
              std::string user_id)
      : filter_words_(std::move(filter_words)),
        filter_(filter),
        user_id_(std::move(user_id)),
        user_name_(GetUserInfo(user_id_)["screen_name"].get<std::string>()) {}
  std::vector<Json> GetLastestWeibos();
  const std::string &GetUserName() const { return user_name_; };
  void CleanBuffer();

 private:
  std::unordered_set<std::string> received_weibo_ids_;
  std::queue<std::string> received_weibo_queue_;
  const std::vector<std::string> filter_words_;
  const bool filter_;
  const std::string user_id_;
  const std::string user_name_;
};

inline std::vector<Json> WeiboSpider::GetLastestWeibos() {
  std::vector<Json> lastest_weibo;
  auto js = GetWeiboJson(user_id_);
  if (js["ok"].get<int>() == 1) {
    auto weibos = js["data"]["cards"];
    for (auto &weibo : weibos) {
      if (weibo["card_type"].get<int>() == 9) {
        auto wb = GetOneWeibo(weibo);
        if (!wb.empty()) {
          for (auto &word : filter_words_) {
            if (wb["text"].get<std::string>().find(word) != std::string::npos ||
                (wb.contains("retweet") &&
                 wb["retweet"]["text"].get<std::string>().find(word) !=
                     std::string::npos)) {
              received_weibo_ids_.emplace(wb["id"].get<std::string>());
              received_weibo_queue_.push(wb["id"].get<std::string>());
            }
          }
          if (received_weibo_ids_.count(wb["id"].get<std::string>())) {
            continue;
          }
          if (!filter_ || !wb.contains("retweet")) {
            received_weibo_queue_.push(wb["id"].get<std::string>());
            received_weibo_ids_.emplace(wb["id"].get<std::string>());
            lastest_weibo.push_back(std::move(wb));
          }
        }
      }
    }
  }
  return lastest_weibo;
}

inline void WeiboSpider::CleanBuffer() {
  while(received_weibo_queue_.size() > 10) {
    received_weibo_ids_.erase(received_weibo_queue_.front());
    received_weibo_queue_.pop();
  }
}

}  // namespace weibo
}  // namespace module
}  // namespace white