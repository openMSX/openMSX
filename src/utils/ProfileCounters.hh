#ifndef PROFILECOUNTERS_HH
#define PROFILECOUNTERS_HH

#include "enumerate.hh"
#include <array>
#include <iostream>

//
// Quick and dirty reflection on C++ enums (in the future replace with c++23 reflexpr).
//

// Print an enum type as a string, for example:
//   os << EnumTypeName<SomeEnumType>() << '\n';
template<typename E> struct EnumTypeName {};
// You _must_ overload 'operator<<' for all enum types for which you want this to work.
template<typename E> std::ostream& operator<<(std::ostream& os, EnumTypeName<E>) = delete;

// Print an enum value as a string, for example:
//   os << EnumValueName{myEnumValue} << '\n';
template<typename E> struct EnumValueName
{
	EnumValueName(E e_) : e(e_) {}
	E e;
};
// You _must_ overload 'operator<<' for all enum types for which you want this to work.
template<typename E> std::ostream& operator<<(std::ostream& os, EnumValueName<E> evn) = delete;




// A collection of (simple) profile counters:
// - Counters start at zero.
// - An individual counter can be incremented by 1 via 'tick(<counter-id>)'.
// - When this 'ProfileCounters' object is destoyed it prints the value of each
//   counter.
//
// Template parameters:
// - bool ENABLED:
//    when false, the optimizer should be able to completely optimize-out all
//    profile related code
// - typename ENUM:
//    Must be a c++ enum (or enum class) which satisfies the following requirements:
//    * The numerical values must be 0, 1, ... IOW the values must be usable as
//      indices in an array. (This is automatically the case if you don't
//      manually assign numeric values).
//    * There must be an enum value with the name 'NUM' which has a numerical
//      value equal to the number of other values in this enum. (This is
//      automatically the case if you put 'NUM' last in the list of enum
//      values).
//    * You must overload the following two functions for the type 'ENUM':
//          std::ostream& operator<<(std::ostream& os, EnumTypeName<ENUM>);
//          std::ostream& operator<<(std::ostream& os, EnumValueName<ENUM> evn);
//
// Example usage:
//    enum class WidgetProfileCounter {
//        CALCULATE,
//        INVALIDATE,
//        NUM // must be last
//    };
//    std::ostream& operator<<(std::ostream& os, EnumTypeName<WidgetProfileCounter>)
//    {
//        return os << "WidgetProfileCounter";
//    }
//    std::ostream& operator<<(std::ostream& os, EnumValueName<WidgetProfileCounter> evn)
//    {
//        std::string_view names[size_t(WidgetProfileCounter::NUM)] = {
//            "calculate"sv, "invalidate"sv
//        };
//        return os << names[size_t(evn.e)];
//    }
//
//    constexpr bool PRINT_WIDGET_PROFILE = true; // possibly depending on some #define
//
//    struct Widget : private ProfileCounters<PRINT_WIDGET_PROFILE, WidgetProfileCounter>
//    {
//        void calculate() {
//            tick(WidgetProfileCounter::CALCULATE);
//            // ...
//        }
//        void invalidate() {
//            tick(WidgetProfileCounter::CALCULATE);
//            // ...
//        }
//    };

template<bool ENABLED, typename ENUM>
class ProfileCounters
{
public:
	// Print counters on destruction.
	~ProfileCounters() {
		std::cout << "-- Profile counters: " << EnumTypeName<ENUM>() << " -- " << static_cast<void*>(this) << '\n';
		for (auto [i, count] : enumerate(counters)) {
			std::cout << EnumValueName{ENUM(i)} << ": " << count << '\n';
		}
	}

	// Increment
	void tick(ENUM e) const {
		++counters[size_t(e)];
	}

private:
	static constexpr auto NUM = size_t(ENUM::NUM); // value 'ENUM::NUM' must exist
	mutable std::array<unsigned, NUM> counters = {};
};


// Specialization for when profiling is disabled.
// This class is simple enough so that it can be completely optimized away.
template<typename ENUM>
class ProfileCounters<false, ENUM>
{
public:
	void tick(ENUM) const { /*nothing*/ }
};

#endif
