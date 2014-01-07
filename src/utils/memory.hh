#ifndef MEMORY_HH
#define MEMORY_HH

#include <memory>
#include <utility>

// See this blog for a motivation for the make_unique() function:
//    http://herbsutter.com/gotw/_102/
// This function is part of the c++14 standard draft. When enough compilers
// support that new standard we can replace our version with std::make_unique.

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&& ...args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#endif
