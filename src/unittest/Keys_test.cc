#include "catch.hpp"
#include "SDLKey.hh"

using namespace openmsx;

static bool equal(const SDLKey& x, const SDLKey& y)
{
	if (x.sym.sym      != y.sym.sym)      return false;
	if (x.sym.scancode != y.sym.scancode) return false;
	if (x.sym.mod      != y.sym.mod)      return false;
	if (x.sym.unused   != y.sym.unused)   return false;
	if (x.down         != y.down)         return false;
	return true;
}

static void test(const std::string& name1, SDLKey code1, bool canonical)
{
	std::string name2 = code1.toString();
	if (canonical) {
		CHECK(name2 == name1);
	}

	std::optional<SDLKey> code2 = SDLKey::fromString(name1);
	CHECK(code2);
	CHECK(equal(*code2, code1));

	std::string name3 = code2->toString();
	CHECK(name3 == name2);

	std::optional<SDLKey> code3 = SDLKey::fromString(name2);
	CHECK(code3);
	CHECK(equal(*code3, *code2));
}

TEST_CASE("Keys")
{
	test("P", SDLKey::createDown(SDLK_P), true);
	test("p", SDLKey::createDown(SDLK_P), false);
	test("1", SDLKey::createDown(SDLK_1), true);
	test("z", SDLKey::createDown(SDLK_Z), false);
	test("AT", SDLKey::createDown(SDLK_AT), false);
	test("@", SDLKey::createDown(SDLK_AT), true);
	test("Hash", SDLKey::createDown(SDLK_HASH), false);
	test("slash", SDLKey::createDown(SDLK_SLASH), false);
	test("EXCLAIM", SDLKey::createDown(SDLK_EXCLAIM), false);
	test("!", SDLKey::createDown(SDLK_EXCLAIM), true);
	test("PeRiOd", SDLKey::createDown(SDLK_PERIOD), false);
	test("CARET", SDLKey::createDown(SDLK_CARET), false);
	test("delete", SDLKey::createDown(SDLK_DELETE), false);
	test("Keypad 7", SDLKey::createDown(SDLK_KP_7), true);
	test("KP7", SDLKey::createDown(SDLK_KP_7), false);
	test("KP_ENTER", SDLKey::createDown(SDLK_KP_ENTER), false);
	test("up", SDLKey::createDown(SDLK_UP), false);
	test("End", SDLKey::createDown(SDLK_END), false);
	test("pageup", SDLKey::createDown(SDLK_PAGEUP), false);
	test("PageDown", SDLKey::createDown(SDLK_PAGEDOWN), true);
	test("PAGEDOWN", SDLKey::createDown(SDLK_PAGEDOWN), false);
	test("F8", SDLKey::createDown(SDLK_F8), true);
	test("RCTRL", SDLKey::createDown(SDLK_RCTRL), false);
	test("Right Ctrl", SDLKey::createDown(SDLK_RCTRL), true);

	test("P+SHIFT", SDLKey::create(SDLK_P, true, SDL_KMOD_SHIFT), true);
	test("q,shift", SDLKey::create(SDLK_Q, true, SDL_KMOD_SHIFT), false);
	test("R/shift", SDLKey::create(SDLK_R, true, SDL_KMOD_SHIFT), false);
	test("ctrl+c", SDLKey::create(SDLK_C, true, SDL_KMOD_CTRL), false);
	test("ALT/E", SDLKey::create(SDLK_E, true, SDL_KMOD_ALT), false);
	test("E+ALT", SDLKey::create(SDLK_E, true, SDL_KMOD_ALT), true);
	test("5,release", SDLKey::create(SDLK_5, false), false);
}
