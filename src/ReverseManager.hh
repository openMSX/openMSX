// $Id$

#ifndef REVERSEMANGER_HH
#define REVERSEMANGER_HH

#include "Schedulable.hh"
#include "EmuTime.hh"
#include "shared_ptr.hh"
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class ReverseCmd;
class MemBuffer;

class ReverseManager : private Schedulable
{
public:
	ReverseManager(MSXMotherBoard& motherBoard);
	~ReverseManager();

private:
	struct ReverseChunk {
		// default copy constructor and assignment operator are OK
		ReverseChunk(EmuTime::param time_) : time(time_) {}

		EmuTime time;
		// TODO use unique_ptr in the future (c++0x), or hold
		//      MemBuffer by value and make it moveable
		shared_ptr<MemBuffer> savestate;
	};
	typedef std::vector<ReverseChunk> Chunks;

	struct ReverseHistory {
		ReverseHistory();
		void swap(ReverseHistory& other);
		void clear();

		Chunks chunks;
		unsigned totalSize;
	};

	std::string start();
	std::string stop();
	std::string status();
	std::string go(const std::vector<std::string>& tokens);

	void transferHistory(ReverseHistory& oldHistory);
	void schedule(EmuTime::param time);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;


	MSXMotherBoard& motherBoard;
	const std::auto_ptr<ReverseCmd> reverseCmd;
	ReverseHistory history;
	bool collecting;

	friend class ReverseCmd;
};

} // namespace openmsx

#endif
