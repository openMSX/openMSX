#ifndef __KEYEVENTINSERTER_HH__
#define __KEYEVENTINSERTER_HH__

#include "emutime.hh"
#include "Scheduler.hh"

#include <string>

class KeyEventInserterEvent: public Schedulable
{
	public:
	KeyEventInserterEvent(int key, bool up);
	void executeUntilEmuTime(const Emutime &time);
	private:
	int key;
	bool up;
};

class KeyEventInserter
{
	public:
	KeyEventInserter();
	KeyEventInserter &operator<<(std::string &str);
	KeyEventInserter &operator<<(const char* cstr);
	void flush(uint64 offset=0);
	private:
	std::string buffer;
};


// this stream ONLY accepts the normal basic ascii and \n
static KeyEventInserter keyi;

#endif
