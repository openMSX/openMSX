// $Id$

#include "KeyEventInserter.hh"
#include "MSXCPU.hh"
#include "SDLEventInserter.hh"
#include "EmuTime.hh"

KeyEventInserter::KeyEventInserter()
{
}

KeyEventInserter &KeyEventInserter::operator<<(std::string &str)
{
	buffer += str;
	return *this;
}

KeyEventInserter &KeyEventInserter::operator<<(const char *cstr)
{
	std::string str(cstr);
	return operator<<(str);
}

void KeyEventInserter::flush()
{
	SDL_Event event;
	EmuTimeFreq<10> time;	// 10 Hz
	time = MSXCPU::instance()->getCurrentTime();
	for (unsigned i=0; i<buffer.length(); i++) {
		event.key.keysym.sym = (SDLKey)buffer[i];
		event.type = SDL_KEYDOWN;
		new SDLEventInserter(event, time);
		time++;
		event.type = SDL_KEYUP;
		new SDLEventInserter(event, time);
		time++;
	}
	buffer = "";
}
