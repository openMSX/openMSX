// $Id$

#include "KeyEventInserter.hh"
#include "MSXCPU.hh"
#include "SDLEventInserter.hh"
#include "EmuTime.hh"


KeyEventInserter::KeyEventInserter()
{
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->
		                            getConfigById("KeyEventInserter");
		enter(config->getParameter("keys"));
	} catch(MSXException &e) {
		// do nothing
	}
}

KeyEventInserter &KeyEventInserter::operator<<(const std::string &str)
{
	enter(str);
	return *this;
}

KeyEventInserter &KeyEventInserter::operator<<(const char *cstr)
{
	std::string str(cstr);
	return operator<<(str);
}

void KeyEventInserter::enter(const std::string &str)
{
	SDL_Event event;
	EmuTimeFreq<5> time(MSXCPU::instance()->getCurrentTime());
	time += 10*5; // wait for bootlogo
	for (unsigned i=0; i<str.length(); i++) {
		const SDLKey* events;
		
		events = keymap[str[i]];
		event.type = SDL_KEYDOWN;
		while (*events != (SDLKey)0) {
			event.key.keysym.sym = *events;
			new SDLEventInserter(event, time);
			events++;
		}
		time++;
		
		events = keymap[str[i]];
		event.type = SDL_KEYUP;
		while (*events != (SDLKey)0) {
			event.key.keysym.sym = *events;
			new SDLEventInserter(event, time);
			events++;
		}
		time++;
	}
}

const SDLKey KeyEventInserter::keymap[256][4] = {
	// 0x00
	{}, {}, {}, {}, {}, {}, {}, {},
	{SDLK_BACKSPACE}, {SDLK_TAB}, {}, {}, {}, {SDLK_RETURN}, {}, {},
	// 0x10
	{}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {SDLK_ESCAPE}, {}, {}, {}, {},
	// 0x20
	{SDLK_SPACE}, {SDLK_LSHIFT, SDLK_1}, {SDLK_LSHIFT, SDLK_QUOTE}, {SDLK_LSHIFT, SDLK_3},
	{SDLK_LSHIFT, SDLK_4}, {SDLK_LSHIFT, SDLK_5}, {SDLK_LSHIFT, SDLK_7}, {SDLK_QUOTE},
	{SDLK_LSHIFT, SDLK_9}, {SDLK_LSHIFT, SDLK_0}, {SDLK_LSHIFT, SDLK_8}, {SDLK_LSHIFT, SDLK_EQUALS},
	{SDLK_COMMA},          {SDLK_MINUS},          {SDLK_PERIOD},         {SDLK_SLASH},
	// 0x30
	{SDLK_0}, {SDLK_1}, {SDLK_2}, {SDLK_3}, {SDLK_4}, {SDLK_5}, {SDLK_6}, {SDLK_7},
	{SDLK_8}, {SDLK_9}, {SDLK_LSHIFT, SDLK_SEMICOLON}, {SDLK_SEMICOLON},
	{SDLK_LSHIFT, SDLK_COMMA}, {SDLK_EQUALS}, {SDLK_LSHIFT, SDLK_PERIOD}, {SDLK_LSHIFT, SDLK_SLASH},
	// 0x40
	{SDLK_LSHIFT, SDLK_2}, {SDLK_LSHIFT, SDLK_a}, {SDLK_LSHIFT, SDLK_b}, {SDLK_LSHIFT, SDLK_c},
	{SDLK_LSHIFT, SDLK_d}, {SDLK_LSHIFT, SDLK_e}, {SDLK_LSHIFT, SDLK_f}, {SDLK_LSHIFT, SDLK_g},
	{SDLK_LSHIFT, SDLK_h}, {SDLK_LSHIFT, SDLK_i}, {SDLK_LSHIFT, SDLK_j}, {SDLK_LSHIFT, SDLK_k},
	{SDLK_LSHIFT, SDLK_l}, {SDLK_LSHIFT, SDLK_m}, {SDLK_LSHIFT, SDLK_n}, {SDLK_LSHIFT, SDLK_o},
	// 0x050
	{SDLK_LSHIFT, SDLK_p}, {SDLK_LSHIFT, SDLK_q}, {SDLK_LSHIFT, SDLK_r}, {SDLK_LSHIFT, SDLK_s},
	{SDLK_LSHIFT, SDLK_t}, {SDLK_LSHIFT, SDLK_u}, {SDLK_LSHIFT, SDLK_v}, {SDLK_LSHIFT, SDLK_w},
	{SDLK_LSHIFT, SDLK_x}, {SDLK_LSHIFT, SDLK_y}, {SDLK_LSHIFT, SDLK_z}, {SDLK_LEFTBRACKET},
	{SDLK_BACKSLASH},      {SDLK_RIGHTBRACKET},   {SDLK_LSHIFT, SDLK_6}, {SDLK_LSHIFT, SDLK_MINUS},
	// 0x60
	{SDLK_BACKQUOTE}, {SDLK_a}, {SDLK_b}, {SDLK_c}, {SDLK_d}, {SDLK_e}, {SDLK_f}, {SDLK_g},
	{SDLK_h},         {SDLK_i}, {SDLK_j}, {SDLK_k}, {SDLK_l}, {SDLK_m}, {SDLK_n}, {SDLK_o},
	// 0x70
	{SDLK_p}, {SDLK_q}, {SDLK_r}, {SDLK_s}, {SDLK_t}, {SDLK_u}, {SDLK_v}, {SDLK_w},
	{SDLK_x}, {SDLK_y}, {SDLK_z}, {SDLK_LSHIFT, SDLK_LEFTBRACKET},
	{SDLK_LSHIFT, SDLK_BACKSLASH}, {SDLK_LSHIFT, SDLK_RIGHTBRACKET},
	{SDLK_LSHIFT, SDLK_BACKQUOTE}, {SDLK_DELETE},
	// 0x80 - 0xff
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
};
