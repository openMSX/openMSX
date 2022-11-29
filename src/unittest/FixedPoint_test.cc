#include "catch.hpp"
#include "FixedPoint.hh"
#include "narrow.hh"

using namespace openmsx;

template<unsigned BITS>
static void check(const FixedPoint<BITS>& fp, int expectedRaw)
{
	REQUIRE(fp.getRawValue() == expectedRaw);
	int       i = expectedRaw >> BITS;
	unsigned fi = expectedRaw & ((1 << BITS) - 1);
	float  ff = float (fi) / (1 << BITS);
	double fd = double(fi) / (1 << BITS);
	float  f = narrow_cast<float >(i) + ff;
	double d = narrow_cast<double>(i) + fd;
	CHECK(fp.toInt()            == i);
	CHECK(fp.toFloat()          == f);
	CHECK(fp.toDouble()         == d);
	CHECK(fp.fractAsInt()       == fi);
	CHECK(fp.fractionAsFloat()  == ff);
	CHECK(fp.fractionAsDouble() == fd);
	CHECK(fp.floor().getRawValue() == (i << BITS));
	CHECK(fp.fract().getRawValue() == int(fi));
}

TEST_CASE("FixedPoint")
{
	FixedPoint<2> a(10  ); check(a,  40);
	FixedPoint<2> b(3.5f); check(b,  14);
	FixedPoint<2> c(1.75); check(c,   7);
	FixedPoint<7> d(c);    check(d, 224); // more fractional bits
	FixedPoint<1> e(d);    check(e,   3); // less fractional bits

	check(FixedPoint<3>::roundRatioDown(2, 3), 5); // 2/3 ~= 5/8

	CHECK(a.divAsInt(b) == 2);
	CHECK(b.divAsInt(a) == 0);
	CHECK(b.divAsInt(c) == 2);

	check(a + b, 54);
	check(a + c, 47);
	check(b + c, 21);

	check(a - b, 26); check(b - a, -26);
	check(a - c, 33); check(c - a, -33);
	check(b - c,  7); check(c - b,  -7);

	check(a *  b, 140);
	check(a *  c,  70);
	check(b *  c,  24);
	check(a *  2,  80);
	check(b * -3, -42);
	check(c *  4,  28);

	check(a /  b, 11); check(b / a, 1);
	check(a /  c, 22); check(c / a, 0);
	check(b /  c,  8); check(c / b, 2);
	check(a / -5, -8);
	check(b /  4,  3);
	check(c / -3, -2);

	check(a << 1, 80); check(a >> 3, 5);
	check(b << 2, 56); check(b >> 2, 3);
	check(c << 3, 56); check(c >> 1, 3);

	CHECK(a == a); CHECK(!(a != a));
	CHECK(a != b); CHECK(!(a == b));
	CHECK(b <  a); CHECK(!(b >= a));
	CHECK(b <= a); CHECK(!(b >  a));
	CHECK(a >  b); CHECK(!(a <= b));
	CHECK(a >= b); CHECK(!(a <  b));

	a += b; check(a,  54);
	c -= a; check(c, -47);

	a.addQuantum(); check(a,  55);
	b.addQuantum(); check(b,  15);
	c.addQuantum(); check(c, -46);
}
