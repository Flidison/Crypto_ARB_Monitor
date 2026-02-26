#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace am::str {

inline std::string trim_copy(std::string s) {
    s.erase(s.begin(),
        std::find_if(s.begin(), s.end(),
            [](unsigned char c){ return !std::isspace(c); }));
    s.erase(
        std::find_if(s.rbegin(), s.rend(),
            [](unsigned char c){ return !std::isspace(c); }).base(),
        s.end());
    return s;
}

inline std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return std::toupper(c); });
    return s;
}

