#pragma once
#include <set>
#include <vector>
#include <string>
 
std::vector<std::string_view> SplitIntoWords(std::string_view text);
 
template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    std::string str_buf;
    for (auto& str : strings) {
        str_buf = str;
        if (!str_buf.empty()) {
            non_empty_strings.insert(str_buf);
        }
    }
    return non_empty_strings;
}