#pragma once

#define VERIFY_EXPR(EXPRESSION, ERR_MESSAGE) do {if(EXPRESSION) { std::cout << ERR_MESSAGE << std::endl; __debugbreak();} }while(0)  
