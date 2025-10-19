#pragma once
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>

#define NOT !
#define VERIFY_EXPR(EXPRESSION, ERR_MESSAGE) \
    do \
    { \
        if (EXPRESSION) \
        { \
            std::cout << ERR_MESSAGE << std::endl; \
            __debugbreak(); \
        } \
    } while (0)
#define ASSERT_ABORT(EXPR, MESSAGE) assert(EXPR&& MESSAGE)

#define ASSERT(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ << " line " << __LINE__ << ": " << message \
                      << std::endl; \
            __debugbreak(); \
        } \
    } while (false)

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T>
using Unique = std::unique_ptr<T>;

// Shared pointer helper
template <typename T, typename... Args>
std::shared_ptr<T> make_s(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// Unique pointer helper
template <typename T, typename... Args>
std::unique_ptr<T> make_u(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}
