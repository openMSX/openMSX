// $Id$

#include "ReverseManager.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "Clock.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class ReverseCmd : public SimpleCommand
{
public:
	ReverseCmd(ReverseManager& manager, CommandController& controller);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	ReverseManager& manager;
};


// struct ReverseHistory

ReverseManager::ReverseHistory::ReverseHistory()
	: totalSize(0)
{
}

void ReverseManager::ReverseHistory::swap(ReverseHistory& other)
{
	std::swap(chunks,    other.chunks);
	std::swap(totalSize, other.totalSize);
}

void ReverseManager::ReverseHistory::clear()
{
	Chunks().swap(chunks); // clear() and free storage capacity
	totalSize = 0;
}


// class ReverseManager

ReverseManager::ReverseManager(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, reverseCmd(new ReverseCmd(*this, motherBoard.getCommandController()))
	, collecting(false)
{
}

ReverseManager::~ReverseManager()
{
}

string ReverseManager::start()
{
	if (!collecting) {
		collecting = true;
		executeUntil(getCurrentTime(), 0);
	}
	return "";
}

string ReverseManager::stop()
{
	removeSyncPoint();
	history.clear();
	collecting = false;
	return "";
}

string ReverseManager::status()
{
	string result;
	for (unsigned i = 0; i < history.chunks.size(); ++i) {
		ReverseChunk& chunk = history.chunks[i];
		result += StringOp::toString(i) + ' ';
		result += StringOp::toString((chunk.time - EmuTime::zero).toDouble());
		result += " (" + StringOp::toString(chunk.savestate->getLength()) + ")\n";
	}
	result += "total size: " + StringOp::toString(history.totalSize) + '\n';
	return result;
}

string ReverseManager::go(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned n = StringOp::stringToInt(tokens[2]);
	if (n >= history.chunks.size()) {
		throw CommandException("Out of range");
	}

	Reactor& reactor = motherBoard.getReactor();
	Reactor::Board newBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(*history.chunks[n].savestate);
	in.serialize("machine", *newBoard);

	assert(collecting);
	collecting = false;
	removeSyncPoint();
	newBoard->getReverseManager().transferHistory(history);
	assert(history.chunks.empty());
	assert(history.totalSize == 0);

	reactor.replaceActiveBoard(newBoard); // TODO this board may not be the active board
	return "";
}

void ReverseManager::transferHistory(ReverseHistory& oldHistory)
{
	assert(!collecting);
	assert(history.chunks.empty());
	history.swap(oldHistory);
	collecting = true;
	schedule(getCurrentTime());
}

void ReverseManager::executeUntil(EmuTime::param time, int /*userData*/)
{
	static const unsigned MAX_SIZE = 100 * 1024 * 1024; // 100MB

	assert(collecting);
	while (history.totalSize >= MAX_SIZE) {
		// drop oldest chunk if total size is more than 100MB
		history.totalSize -= history.chunks.front().savestate->getLength();
		history.chunks.erase(history.chunks.begin());
	}

	MemOutputArchive out;
	out.serialize("machine", motherBoard);
	ReverseChunk newChunk(time);
	newChunk.savestate.reset(new MemBuffer(out.stealBuffer()));
	history.chunks.push_back(newChunk);
	history.totalSize += newChunk.savestate->getLength();
	schedule(time);
}

void ReverseManager::schedule(EmuTime::param time)
{
	Clock<1> clock(time);
	clock += 1;
	setSyncPoint(clock.getTime());
}

const string& ReverseManager::schedName() const
{
	static const string NAME = "ReverseManager";
	return NAME;
}


// class ReverseCmd

ReverseCmd::ReverseCmd(ReverseManager& manager_, CommandController& controller)
	: SimpleCommand(controller, "reverse")
	, manager(manager_)
{
}

string ReverseCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing subcommand");
	}
	if (tokens[1] == "start") {
		return manager.start();
	} else if (tokens[1] == "stop") {
		return manager.stop();
	} else if (tokens[1] == "status") {
		return manager.status();
	} else if (tokens[1] == "go") {
		return manager.go(tokens);
	} else {
		throw CommandException("Invalid subcommand");
	}
}

string ReverseCmd::help(const vector<string>& /*tokens*/) const
{
	return "!! this is NOT the final command, this is only for experiments !!\n"
	       "start   start collecting reverse data\n"
	       "stop    stop  collecting\n"
	       "status  give overview of collected data\n"
	       "go <n>  go back to a previously collected point\n";
}

void ReverseCmd::tabCompletion(vector<string>& /*tokens*/) const
{
	// TODO
}

} // namespace openmsx
