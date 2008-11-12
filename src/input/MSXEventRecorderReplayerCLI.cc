// $Id$

#include "MSXEventRecorderReplayerCLI.hh"
#include "CommandLineParser.hh"
#include "CLIOption.hh"
#include "MSXEventRecorder.hh"
#include "MSXEventReplayer.hh"
#include "MSXMotherBoard.hh"

using std::string;
using std::deque;

namespace openmsx {

class RecordOption : public CLIOption
{
public:
	explicit RecordOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
	std::auto_ptr<MSXEventRecorder> eventRecorder;
};


class ReplayOption : public CLIOption
{
public:
	explicit ReplayOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
	std::auto_ptr<MSXEventReplayer> eventReplayer;
};


// class MSXEventRecorderReplayerCLI

MSXEventRecorderReplayerCLI::MSXEventRecorderReplayerCLI(
		CommandLineParser& commandLineParser)
	: recordOption(new RecordOption(commandLineParser))
	, replayOption(new ReplayOption(commandLineParser))
{
	commandLineParser.registerOption("-record", *recordOption);
	commandLineParser.registerOption("-replay", *replayOption);
}

MSXEventRecorderReplayerCLI::~MSXEventRecorderReplayerCLI()
{
}


// class RecordOption

RecordOption::RecordOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool RecordOption::parseOption(const string& option, deque<string>& cmdLine)
{
	const string logFileName = getArgument(option, cmdLine);
	eventRecorder.reset(new MSXEventRecorder(
		parser.getMotherBoard()->getMSXEventDistributor(), logFileName));
	return true;
}

const string& RecordOption::optionHelp() const
{
	static const string text("Record all input events to a file");
	return text;
}


// class ReplayOption

ReplayOption::ReplayOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool ReplayOption::parseOption(const string& option, deque<string>& cmdLine)
{
	const string logFileName = getArgument(option, cmdLine);
	eventReplayer.reset(new MSXEventReplayer(
		parser.getMotherBoard()->getScheduler(),
		parser.getMotherBoard()->getMSXEventDistributor(),
		logFileName));
	return true;
}

const string& ReplayOption::optionHelp() const
{
	static const string text("Replay all input events from a file");
	return text;
}

} // namespace openmsx
