#include "KeyEventInserter.hh"

#include "MSXCPU.hh"

KeyEventInserter keyi();

KeyEventInserterEvent::KeyEventInserterEvent()
{
}

void
KeyEventInserterEvent::executeUntilEmuTime(const Emutime &time)
{
	// do my stuff on the PPI?!
	delete this;
}

KeyEventInserter::KeyEventInserter()
{
}

KeyEventInserter &KeyEventInserter::operator<<(std::string &str)
{
	// append
}

void KeyEventInserter::flush()
{
	// create KeyEventInserterSchedulable's
	// for each key and schedule them
	//
	// prevTime = MSXCPU::instance()->getCurrentTime();
}
