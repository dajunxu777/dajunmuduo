#pragma once

#include<iostream>
#include<string>
#include "time.h"

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(time_t microSecondsSinceEpoch); //防止隐式类型转换
    static Timestamp now();
    std::string toString() const;
private:
    time_t microSecondsSinceEpoch_;
};
