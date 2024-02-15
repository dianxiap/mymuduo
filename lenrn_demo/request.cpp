#include <iostream>
#include <string>
#include <regex>

int main()
{
    //HTTP请求格式： GET /xxxxx/login?user=xiaoming&pass=123123 HTTP/1.1\r\n
    std::string str="get /xxxxx/login?user=xiaoming&pass=123123 HTTP/1.1\r\n";
    std::smatch matchs;
    //请求方法的匹配  GET HEAD POST PUT DELETE ...
    std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)",std::regex::icase);
    // (GET|HEAD|POST|PUT|DELETE) 表示匹配并提取其中任意一个字符串
    // ([^?]*)     [^?]匹配非问号字符，后边的*表示0次或多次
    // \\?(.*)   \\?  表示原始的？字符 (.*)表示提取?之后的任意字符0次或多次，直到遇到空格
    // HTTP/1\\.[01]  表示匹配以HTTP/1.开始，后边有个0或1的字符串
    // (?:\n|\r\n)?   （?: ...） 表示匹配某个格式字符串，但是不提取， 最后的？表示的是匹配前边的表达式0次或1次

    bool ret=std::regex_match(str,matchs,e);
    if(ret==false)return -1;

    for(int i=0;i<matchs.size();i++)
    {
        std::cout<<matchs[i]<<std::endl;
    }   
}