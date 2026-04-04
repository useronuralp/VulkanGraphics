#pragma once

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <vector>
#include <vulkan/vulkan.h>

//////////////////////////////////////////////////////////////
// ANSI Color Codes
//////////////////////////////////////////////////////////////
constexpr const char* RED_TEXT    = "\033[31m";
constexpr const char* ORANGE_TEXT = "\033[38;5;208m"; // true orange in 256-color ANSI
constexpr const char* YELLOW_TEXT = "\033[33m";
constexpr const char* GREEN_TEXT  = "\033[32m";
constexpr const char* CYAN_TEXT   = "\033[36m";
constexpr const char* BLUE_TEXT   = "\033[34m";
constexpr const char* RESET_TEXT  = "\033[0m";
constexpr const char* BOLD_TEXT   = "\033[1m";

//////////////////////////////////////////////////////////////
// General Macros
//////////////////////////////////////////////////////////////
#define MAX_FRAMES_IN_FLIGHT 3
#define NOT                  !

#define VERIFY_EXPR(EXPR, ERR_MESSAGE) \
    do \
    { \
        if (EXPR) \
        { \
            std::cout << ERR_MESSAGE << std::endl; \
            __debugbreak(); \
        } \
    } while (0)

#define ASSERT_ABORT(EXPR, MESSAGE) assert((EXPR) && MESSAGE)

#define ENSURE(CONDITION, MESSAGE) \
    do \
    { \
        if (!(CONDITION)) \
        { \
            std::cerr << "Assertion `" #CONDITION "` failed in " << __FILE__ << " line " << __LINE__ << ": " << MESSAGE << std::endl; \
            __debugbreak(); \
        } \
    } while (false)

//////////////////////////////////////////////////////////////
// Smart Pointer Aliases
//////////////////////////////////////////////////////////////
template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T>
using Unique = std::unique_ptr<T>;

template <typename T, typename... Args>
inline Ref<T> make_s(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline Unique<T> make_u(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

//////////////////////////////////////////////////////////////
// Logging Helpers
//////////////////////////////////////////////////////////////

inline void PrintInfo(const std::string& msg)
{
    std::cout << BOLD_TEXT << BLUE_TEXT << "[INFO] " << RESET_TEXT << msg << std::endl;
}

inline void PrintWarning(const std::string& msg)
{
    std::cerr << BOLD_TEXT << YELLOW_TEXT << "[WARNING] " << RESET_TEXT << msg << std::endl;
}

inline void PrintError(const std::string& msg)
{
    std::cerr << BOLD_TEXT << RED_TEXT << "[ERROR] " << RESET_TEXT << msg << std::endl;
}

inline void PrintRenderGraph(const std::string& msg)
{
    std::cout << BOLD_TEXT << CYAN_TEXT << "[RenderGraph] " << RESET_TEXT << msg << std::endl;
}
