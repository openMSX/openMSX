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
	test("P",          SDLKey::createDown(SDLK_p,        SDL_SCANCODE_UNKNOWN), true);
	test("p",          SDLKey::createDown(SDLK_p,        SDL_SCANCODE_UNKNOWN), false);
	test("1",          SDLKey::createDown(SDLK_1,        SDL_SCANCODE_UNKNOWN), true);
	test("z",          SDLKey::createDown(SDLK_z,        SDL_SCANCODE_UNKNOWN), false);
	test("AT",         SDLKey::createDown(SDLK_AT,       SDL_SCANCODE_UNKNOWN), false);
	test("@",          SDLKey::createDown(SDLK_AT,       SDL_SCANCODE_UNKNOWN), true);
	test("Hash",       SDLKey::createDown(SDLK_HASH,     SDL_SCANCODE_UNKNOWN), false);
	test("slash",      SDLKey::createDown(SDLK_SLASH,    SDL_SCANCODE_UNKNOWN), false);
	test("EXCLAIM",    SDLKey::createDown(SDLK_EXCLAIM,  SDL_SCANCODE_UNKNOWN), false);
	test("!",          SDLKey::createDown(SDLK_EXCLAIM,  SDL_SCANCODE_UNKNOWN), true);
	test("PeRiOd",     SDLKey::createDown(SDLK_PERIOD,   SDL_SCANCODE_UNKNOWN), false);
	test("CARET",      SDLKey::createDown(SDLK_CARET,    SDL_SCANCODE_UNKNOWN), false);
	test("delete",     SDLKey::createDown(SDLK_DELETE,   SDL_SCANCODE_UNKNOWN), false);
	test("Keypad 7",   SDLKey::createDown(SDLK_KP_7,     SDL_SCANCODE_UNKNOWN), true);
	test("KP7",        SDLKey::createDown(SDLK_KP_7,     SDL_SCANCODE_UNKNOWN), false);
	test("KP_ENTER",   SDLKey::createDown(SDLK_KP_ENTER, SDL_SCANCODE_UNKNOWN), false);
	test("up",         SDLKey::createDown(SDLK_UP,       SDL_SCANCODE_UNKNOWN), false);
	test("End",        SDLKey::createDown(SDLK_END,      SDL_SCANCODE_UNKNOWN), false);
	test("pageup",     SDLKey::createDown(SDLK_PAGEUP,   SDL_SCANCODE_UNKNOWN), false);
	test("PageDown",   SDLKey::createDown(SDLK_PAGEDOWN, SDL_SCANCODE_UNKNOWN), true);
	test("PAGEDOWN",   SDLKey::createDown(SDLK_PAGEDOWN, SDL_SCANCODE_UNKNOWN), false);
	test("F8",         SDLKey::createDown(SDLK_F8,       SDL_SCANCODE_UNKNOWN), true);
	test("RCTRL",      SDLKey::createDown(SDLK_RCTRL,    SDL_SCANCODE_UNKNOWN), false);
	test("Right Ctrl", SDLKey::createDown(SDLK_RCTRL,    SDL_SCANCODE_UNKNOWN), true);

	test("P+SHIFT",   SDLKey::create(SDLK_p, SDL_SCANCODE_UNKNOWN, true, KMOD_SHIFT), true);
	test("q,shift",   SDLKey::create(SDLK_q, SDL_SCANCODE_UNKNOWN, true, KMOD_SHIFT), false);
	test("R/shift",   SDLKey::create(SDLK_r, SDL_SCANCODE_UNKNOWN, true, KMOD_SHIFT), false);
	test("ctrl+c",    SDLKey::create(SDLK_c, SDL_SCANCODE_UNKNOWN, true, KMOD_CTRL ), false);
	test("ALT/E",     SDLKey::create(SDLK_e, SDL_SCANCODE_UNKNOWN, true, KMOD_ALT  ), false);
	test("E+ALT",     SDLKey::create(SDLK_e, SDL_SCANCODE_UNKNOWN, true, KMOD_ALT  ), true);
	test("5,release", SDLKey::create(SDLK_5, SDL_SCANCODE_UNKNOWN, false           ), false);
}
