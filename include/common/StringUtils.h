#pragma once

#include <cctype>
#include <string>
#include <utility>

namespace am::str {

inline std::string trim_copy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

inline std::string upper_copy(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

inline std::string upper_trim_copy(std::string s) {
    return upper_copy(trim_copy(std::move(s)));
}

} // namespace am::str
