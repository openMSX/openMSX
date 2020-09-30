#ifndef SCOPE_EXIT_HH
#define SCOPE_EXIT_HH

// Simplified version of
//    https://en.cppreference.com/w/cpp/experimental/scope_exit
//
// Typically used to execute some cleanup action whenever a certain scope is
// left. Either via the fall-through code path, a return/break/continue or via
// an exception.
//
// Example usage:
//     bool f() {
//         char* ptr = some_c_style_function_that_returns_an_owning_pointer()
//         scope_exit e([&]{ free(ptr); });
//         if (some_check(ptr)) return false; // 1
//         stuff_which_might_throw(); // 2
//         return true; // 3
//     }
//
//     Note: 'free(ptr)' is called when we exit the function via 1) 2) or 3)

#include <utility>

template<typename FUNC>
class scope_exit
{
public:
    explicit scope_exit(FUNC&& action_)
        : action(std::forward<FUNC>(action_))
    {
    }

    ~scope_exit()
    {
        action();
    }

private:
    [[no_unique_address]] FUNC action;
};

#endif
