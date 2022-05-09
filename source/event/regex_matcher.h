#ifndef MIGANGBOT_EVENT_EVENT_REGEX_MATCHER_H_
#define MIGANGBOT_EVENT_EVENT_REGEX_MATCHER_H_

#include <initializer_list>
#include <string_view>
#include <vector>

#include <jpcre2.hpp>

#include "event/type.h"

namespace white {

using jp = jpcre2::select<char>;

class RegexMatcher {
 public:
  RegexMatcher(const std::initializer_list<std::string> &patterns,
               const plugin_func &&func)
      : func_(std::move(func)) {
    for (const auto &pattern : patterns)
      regex_.push_back(jp::Regex(pattern, "mSi"));
  }

  RegexMatcher(const std::initializer_list<std::string> &patterns,
               const plugin_func &func)
      : func_(func) {
    for (const auto &pattern : patterns)
      regex_.push_back(jp::Regex(pattern, "mSi"));
  }

  bool Check(const std::string &str) noexcept {
    for (auto &re : regex_)
      if (re.match(str)) return true;
    return false;
  }

  const plugin_func &GetFunc() const noexcept { return func_; }

 private:
  std::vector<jp::Regex> regex_;
  const plugin_func func_;
};

}  // namespace white

#endif