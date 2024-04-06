#include "catch.hpp"
#include "ScopedAssign.hh"

TEST_CASE("ScopedAssign, local")
{
	int l = 1;
	CHECK(l == 1);
	{
		ScopedAssign sa1(l, 2);
		CHECK(l == 2);
		{
			ScopedAssign sa2(l, 3);
			CHECK(l == 3);
		}
		CHECK(l == 2);
	}
	CHECK(l == 1);
}

int g;
static void testAssign()
{
	CHECK(g == 2);
	ScopedAssign sa(g, 3);
	CHECK(g == 3);
}
TEST_CASE("ScopedAssign, global")
{
	g = 1;
	CHECK(g == 1);
	{
		ScopedAssign sa(g, 2);
		CHECK(g == 2);
		testAssign();
		CHECK(g == 2);
	}
	CHECK(g == 1);
}
