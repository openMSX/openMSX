// $Id$

#ifndef REVERSEMANGER_HH
#define REVERSEMANGER_HH

#include "Schedulable.hh"
#include "EmuTime.hh"
#include "shared_ptr.hh"
#include <vector>
#include <map>
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
		ReverseChunk() : time(EmuTime::zero) {}

		EmuTime time;
		// TODO use unique_ptr in the future (c++0x), or hold
		//      MemBuffer by value and make it moveable
		shared_ptr<MemBuffer> savestate;
	};
	typedef std::map<unsigned, ReverseChunk> Chunks;

	struct ReverseHistory {
		ReverseHistory();
		void swap(ReverseHistory& other);
		void clear();

		Chunks chunks;
	};

	std::string start();
	std::string stop();
	std::string status();
	std::string go(const std::vector<std::string>& tokens);

	void transferHistory(ReverseHistory& oldHistory,
                             unsigned oldCollectCount);
	void schedule(EmuTime::param time);
	template<unsigned N> void dropOldSnapshots(unsigned count);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;


	MSXMotherBoard& motherBoard;
	const std::auto_ptr<ReverseCmd> reverseCmd;
	ReverseHistory history;
	unsigned collectCount; // 0     = not collecting
	                       // other = number of snapshot that's about to be taken

	friend class ReverseCmd;
};

} // namespace openmsx

#endif
