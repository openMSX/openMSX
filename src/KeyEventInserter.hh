#ifndef __KEYEVENTINSERTER_HH__
#define __KEYEVENTINSERTER_HH__

#include "emutime.hh"
#include "Scheduler.hh"

#include <string>

class KeyEventInserterEvent: public Schedulable
{
	public:
	KeyEventInserterEvent();
	void executeUntilEmuTime(const Emutime &time);
};

class KeyEventInserter
{
	public:
	KeyEventInserter();
	KeyEventInserter &operator<<(std::string &str);
	void flush();
	private:
	std::string buffer;
};


// this stream ONLY accepts the normal basic ascii and \n
static KeyEventInserter keyi();

#endif
