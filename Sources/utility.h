#ifndef MIGANGBOTCPP_UTILITY_H_
#define MIGANGBOTCPP_UTILITY_H_

#include <iterator>
#include <string>
#include <codecvt>
#include <chrono>
#include <ctime>
#include <fmt/chrono.h>
#include <algorithm>
#include <random>
#include "utf8.h"

namespace white
{

inline std::string ReverseUTF8(const std::string &str)
{
    std::u16string str_u16 = utf8::utf8to16(str);
    std::reverse(str_u16.begin(), str_u16.end());
    return utf8::utf16to8(str_u16);
}

inline bool IsDigitStr(const std::string &str)
{
    return std::all_of(str.begin(), str.end(), isdigit);
}

// https://stackoverflow.com/questions/6942273/how-to-get-a-random-element-from-a-c-container
template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

inline int GetRandomNum(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return std::uniform_int_distribution<>(min, max)(gen);
}

namespace datetime
{

inline std::string GetCurrentTime()
{
    std::time_t result = std::time(nullptr);
    std::tm *tm = localtime(&result);
    return fmt::format("{:%Y-%m-%d %H:%M:%S}", *tm);
}

inline std::time_t GetTimeStampMs()
{
    auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now());
    return tp.time_since_epoch().count();
}

inline std::time_t GetTimeStampS()
{
    auto tp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now());
    return tp.time_since_epoch().count();
}

inline std::time_t LastSecondOfToday()
{
    std::time_t result = std::time(nullptr);
    std::tm *tm = localtime(&result);
    tm->tm_hour = 23;
    tm->tm_min = 59;
    tm->tm_sec = 59;
    return std::mktime(tm);
}

} // namespace datatime

} // namespace white

#endif