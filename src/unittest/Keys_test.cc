#include "catch.hpp"
#include "SDLKey.hh"

using namespace openmsx::Keys;

static void test(const std::string& name1, KeyCode code1, bool canonical)
{
	std::string name2 = getName(code1);
	if (canonical) {
		CHECK(name2 == name1);
	}

	KeyCode code2 = getCode(name1);
	CHECK(code2 == code1);

	std::string name3 = getName(code2);
	CHECK(name3 == name2);

	KeyCode code3 = getCode(name2);
	CHECK(code3 == code2);
}

TEST_CASE("Keys")
{
	test("P", K_P, true);
	test("p", K_P, false);
	test("1", K_1, true);
	test("z", K_Z, false);
	test("AT", K_AT, true);
	test("Hash", K_HASH, false);
	test("slash", K_SLASH, false);
	test("EXCLAIM", K_EXCLAIM, true);
	test("PeRiOd", K_PERIOD, false);
	test("CARET", K_CARET, true);
	test("delete", K_DELETE, false);
	test("KP7", K_KP7, true);
	test("KP_ENTER", K_KP_ENTER, true);
	test("up", K_UP, false);
	test("End", K_END, false);
	test("pageup", K_PAGEUP, false);
	test("PAGEDOWN", K_PAGEDOWN, true);
	test("F8", K_F8, true);
	test("RCTRL", K_RCTRL, true);

	test("P+SHIFT", combine(K_P, KM_SHIFT), true);
	test("q,shift", combine(K_Q, KM_SHIFT), false);
	test("R/shift", combine(K_R, KM_SHIFT), false);
	test("ctrl+c", combine(K_C, KM_CTRL), false);
	test("ALT/E", combine(K_E, KM_ALT), false);
	test("E+ALT", combine(K_E, KM_ALT), true);
	test("5,release", combine(K_5, KD_RELEASE), false);
}
