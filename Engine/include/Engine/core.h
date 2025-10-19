#pragma once
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>

#define RED_TEXT    "\033[31m"
#define ORANGE_TEXT "\033[38;5;208m" // true orange in 256-color ANSI
#define YELLOW_TEXT "\033[33m"
#define GREEN_TEXT  "\033[32m"
#define CYAN_TEXT   "\033[36m"
#define RESET_TEXT  "\033[0m"
#define BOLD_TEXT   "\033[1m"
#define BLUE_TEXT   "\033[34m"

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
