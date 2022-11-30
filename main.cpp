#include <iostream>
#include <string>

#include "lib/db.hpp"

namespace fmt
{
    std::string to_string(ambry::IndexData data)
    {
        return std::to_string(data.offset) + ":" + std::to_string(data.length) + ":" + std::to_string((int)data.type);
    }
}

#include "fmt.hpp"
#include <cmath>

using namespace ambry;

int main()
{
    DB db("test");

    Result result = db.open();

    if (!result.ok())
        fmt::fatal("could not open db. {}\n", result.message);

    db.set("one", 1);
    db.set("two", 2);
    //db.set("str", "hello");

    db.erase("one");
    db.erase("two");

    db.set("all", 5);
    db.set("epic", 10);

    // Result res = db.set("one", 30);

    // fmt::println("res {}", res.ok());

    // Value value = db.get("one");

    // if (value.index() == Int)
    //     fmt::println("value: {}", std::get<2>(value));

    //fmt::println("str_value: {}", std::get<1>(db.get("str")));

    fmt::println("index:\n{}", db.m_index);
    fmt::println("data:\n{}", db.m_data.m_data);
}