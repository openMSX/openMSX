#include "KeyEventInserter.hh"

#include "MSXCPU.hh"
#include "MSXPPI.hh"

//KeyEventInserter keyi;

KeyEventInserterEvent::KeyEventInserterEvent(int key_, bool up_):key(key_),up(up_)
{
}

void
KeyEventInserterEvent::executeUntilEmuTime(const Emutime &time)
{
	// we won't do it this way: MSXPPI::instance()->injectKey(key, up);
	SDL_Event event;
	event.key.keysym.sym = static_cast<enum SDLKey>(key);
	event.type = up?SDL_KEYUP:SDL_KEYDOWN;
	SDL_PushEvent(&event);

	delete this;
}

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

void KeyEventInserter::flush(uint64 offset)
{
	// create KeyEventInserterSchedulable's
	// for each key and schedule them
	//
	// prevTime = MSXCPU::instance()->getCurrentTime();
	buffer = "";
}
