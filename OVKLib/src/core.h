#pragma once
#include <cassert>
#include <memory>
#include <iostream>
#define VERIFY_EXPR(EXPRESSION, ERR_MESSAGE) do {if(EXPRESSION) { std::cout << ERR_MESSAGE << std::endl; __debugbreak();} }while(0)
#define ASSERT_ABORT(EXPR, MESSAGE) assert(EXPR && MESSAGE)

#define ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            __debugbreak(); \
        } \
    } while (false)
namespace OVK
{
	template<typename T>
	using Ref = std::shared_ptr<T>;
	
	template <typename T>
	using Unique = std::unique_ptr<T>;
}
