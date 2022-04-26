#ifndef MIGANGBOTCPP_MESSAGE_MESSAGE_SEGMENT_H_
#define MIGANGBOTCPP_MESSAGE_MESSAGE_SEGMENT_H_

#include <string>
#include <string_view>
#include <fmt/core.h>

namespace white
{
namespace message_segment
{

inline std::string image(const std::string_view &view)
{
    return fmt::format("[CQ:image,file={}]", view);
}

} // namespace message_segment
} // namespace white

#endif