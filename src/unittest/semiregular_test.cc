#include "catch.hpp"
#include "semiregular.hh"
#include <memory>


// Mockup for an transform-iterator containing a transformation function (e.g.
// a lambda).
template<typename Op> struct Iter {
	Iter() = default;
	Iter(Op op_) : op(std::move(op_)) {}
	semiregular_t<Op> op; // wrap 'Op' in semiregular_t<T>
};

TEST_CASE("semiregular")
{
	// This doesn't CHECK() anything, it only tests that the code compiles.
	SECTION("semiregular_t<lambda>") {
		int g = 42;

		// create dummy lambda, captures 'g' by-reference.
		auto lambda = [&](int i) { return i + g; };
		auto l2 = lambda; (void)l2; // lambda is copy-constructible
		// l2 = lambda; // ok, doesn't compile: lambda is not copy-assignable
		// decltype(lambda) l3; // ok, doesn't compile: not default-constructible

		// create iterator containing this lambda
		auto iter1 = Iter(std::move(lambda));
		auto iter2 = iter1; // ok, copy-constructible
		iter2 = iter1; // ok, copy-assignable
		decltype(iter1) iter3; // ok, default-constructible
		(void)iter2; (void)iter3;
	}
	SECTION("semiregular_t<unique_ptr<int>>") {
		// unique_ptr
		using T = std::unique_ptr<int>;
		T t1 = std::make_unique<int>(43);
		// T t2 = t1; // ok, doesn't compile: move-only

		// wrapped in semiregular_t<T>
		using ST = semiregular_t<T>;
		ST st1 = std::move(t1);
		// ST st2 = st1; ok, doesn't compile because move-only
		ST st2 = std::move(st1);
		st1 = std::move(st2);
	}
}
