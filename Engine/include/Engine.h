#pragma once
#include <memory>

class MyApplication;
class Engine
{
public:
	void Init();
	void Run();
private:
	void Shutdown();

	MyApplication* app;
};