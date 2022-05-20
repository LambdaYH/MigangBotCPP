#ifndef MIGANGBOT_MODULES_MODULE_INTERFACE_H_
#define MIGANGBOT_MODULES_MODULE_INTERFACE_H_

#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
#include <memory>

#include <nlohmann/json.hpp>
#include <fmt/core.h>

#include "logger/logger.h"
#include "message/utility.h"
#include "permission/permission.h"
#include "utility.h"
#include "event/Registrar.h"

namespace white {
namespace module {

#define ACT_InClass(func) \
  [this](const Event &event, onebot11::ApiBot &bot) { func(event, bot); }

#define ACT_OutClass(func) \
  [](const Event &event, onebot11::ApiBot &bot) { func(event, bot); }

using std::make_pair;

using Config = YAML::Node;

class Module {
 public:
  // 如果没有配置文件，就留空
  Module(const std::string_view &config_file = "",
         const std::string_view &config_example = "")
      : config_file_(config_file), config_example_(config_example) {}

  virtual void Register() = 0;

 protected:
  YAML::Node LoadConfig() {
    YAML::Node ret;
    if (config_file_.empty()) {
      LOG_WARN("未配置配置文件路径", config_file_);
      return ret;
    }
    if (!std::string_view(config_file_).ends_with("yml")) {
      LOG_WARN("配置文件格式错误，应当为.yml");
      return ret;
    }
    std::filesystem::path path;
    if (config_file_[0] != '/') path = config::kConfigDir / config_file_;
    if (!std::filesystem::exists(path)) {
      LOG_WARN("无法加载配置文件[{}]", config_file_);
      LOG_INFO("尝试生成配置文件");
      auto parent_path = path.parent_path();
      if (!std::filesystem::exists(parent_path) &&
          !std::filesystem::create_directories(parent_path)) {
        LOG_ERROR("创建配置文件[{}]失败", config_file_);
        return ret;
      }
      std::fstream f(path, std::ios_base::out);
      f << config_example_;
      f.close();
      LOG_INFO("已成功创建[{}]", config_file_);
    }
    ret = YAML::LoadFile(path);
    return ret;
  }

 private:
  const std::string config_file_;
  const std::string config_example_;
};

class ModuleBundle {
 public:
  virtual void Register() = 0;
};

}  // namespace module
}  // namespace white
#endif