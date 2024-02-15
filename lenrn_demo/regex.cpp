#include <iostream>
#include <string>
#include <regex>

int main()
{
    std::string str = "/numbers/1234";
    // 匹配以"/numbers/"为起始，后边跟一个或者多个数字字符的字符串，并且在匹配的过程中
    // 提取匹配到的这个数字字符串
    std::regex e("/numbers/(\\d+)");
    std::smatch matches;
    if (std::regex_match(str, matches, e))
    {
        for (size_t i = 1; i < matches.size(); ++i) {
            std::cout << matches[i] << std::endl;  // 匹配的子字符串
        }
    }
    return 0;
}