#include "catch.hpp"
#include "SDLKey.hh"

using namespace openmsx;

static bool equal(const SDLKey& x, const SDLKey& y)
{
	if (x.keycode  != y.keycode)  return false;
	if (x.scancode != y.scancode) return false;
	if (x.mod      != y.mod)      return false;
	if (x.down     != y.down)     return false;
	return true;
}

static void test(const std::string& name1, SDLKey code1, bool canonicalName = true, bool canonicalCode = true)
{
	// Note: this may take a while before this settles into a stable name/code pair.
	// For example:
	//      name      code
	// 1)  "EXCLAIM"  33       // This is the test input for this function
	// 2)  "!"        33       // "EXCLAIM" is not recognized by SDL3, openMSX support it for bw compat (should we drop it?)
	// 3)  "!"        49       // This behavior is new in SDL3 (different from SDL2): "!" is translated to code 49 (was 33 for SDL2)
	// 4)  "1"        49       // And then code 49 is translated to "1"
	// 5)  "1"        49       // Only here we get the same row as above

	std::string name2 = code1.toString();
	if (canonicalName) CHECK(name2 == name1);

	std::optional<SDLKey> code2 = SDLKey::fromString(name1);
	CHECK(code2);
	if (canonicalCode) CHECK(equal(*code2, code1));

	std::string name3 = code2->toString();
	if (canonicalName) CHECK(name3 == name2);

	std::optional<SDLKey> code3 = SDLKey::fromString(name2);
	CHECK(code3);
	if (canonicalCode) CHECK(equal(*code3, *code2));

	std::string name4 = code3->toString();
	if (canonicalName) CHECK(name4 == name3);

	std::optional<SDLKey> code4 = SDLKey::fromString(name3);
	CHECK(code4);
	CHECK(equal(*code4, *code3));

	std::string name5 = code4->toString();
	CHECK(name5 == name4);
	std::optional<SDLKey> code5 = SDLKey::fromString(name4);
	CHECK(code5);
	CHECK(equal(*code5, *code4));
}

TEST_CASE("Keys")
{
	test("P", SDLKey::createDown(SDLK_P));
	test("p", SDLKey::createDown(SDLK_P), false);
	test("1", SDLKey::createDown(SDLK_1));
	test("z", SDLKey::createDown(SDLK_Z), false);
	test("AT", SDLKey::createDown(SDLK_AT), false, false);
	test("@", SDLKey::createDown(SDLK_AT), false, false);
	test("Hash", SDLKey::createDown(SDLK_HASH), false);
	test("slash", SDLKey::createDown(SDLK_SLASH), false);
	test("EXCLAIM", SDLKey::createDown(SDLK_EXCLAIM), false, false);
	test("!", SDLKey::createDown(SDLK_EXCLAIM), false, false);
	test("PeRiOd", SDLKey::createDown(SDLK_PERIOD), false);
	test("CARET", SDLKey::createDown(SDLK_CARET), false, false);
	test("delete", SDLKey::createDown(SDLK_DELETE), false);
	test("Keypad 7", SDLKey::createDown(SDLK_KP_7));
	test("KP7", SDLKey::createDown(SDLK_KP_7), false);
	test("KP_ENTER", SDLKey::createDown(SDLK_KP_ENTER), false);
	test("up", SDLKey::createDown(SDLK_UP), false);
	test("End", SDLKey::createDown(SDLK_END), false);
	test("pageup", SDLKey::createDown(SDLK_PAGEUP), false);
	test("PageDown", SDLKey::createDown(SDLK_PAGEDOWN));
	test("PAGEDOWN", SDLKey::createDown(SDLK_PAGEDOWN), false);
	test("F8", SDLKey::createDown(SDLK_F8));
	test("RCTRL", SDLKey::createDown(SDLK_RCTRL), false);
	test("Right Ctrl", SDLKey::createDown(SDLK_RCTRL));

	test("P+SHIFT", SDLKey::create(SDLK_P, true, SDL_KMOD_SHIFT));
	test("q,shift", SDLKey::create(SDLK_Q, true, SDL_KMOD_SHIFT), false);
	test("R/shift", SDLKey::create(SDLK_R, true, SDL_KMOD_SHIFT), false);
	test("ctrl+c", SDLKey::create(SDLK_C, true, SDL_KMOD_CTRL), false);
	test("ALT/E", SDLKey::create(SDLK_E, true, SDL_KMOD_ALT), false);
	test("E+ALT", SDLKey::create(SDLK_E, true, SDL_KMOD_ALT));
	test("5,release", SDLKey::create(SDLK_5, false), false);
}
