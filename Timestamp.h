#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public: 
    Timestamp();
    explicit Timestamp(int64_t mircroSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
private:
    int64_t mircroSecondsSinceEpoch_;

};