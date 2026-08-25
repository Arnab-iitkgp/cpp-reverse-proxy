#pragma once
#include<string>
#include<map>

class HTTPRequest{
    public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string>headers;

    void parse(std::string raw_request);
};