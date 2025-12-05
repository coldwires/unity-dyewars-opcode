#pragma once
#include <variant>
#include <cstdio>
namespace Actions {

    struct Move{
        uint64_t id;
    };
    using Actions = std::variant<Move>;
}