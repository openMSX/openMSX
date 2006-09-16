// $Id$

#ifndef MSXEVENTRECORDERREPLAYERCLI_HH
#define MSXEVENTRECORDERREPLAYERCLI_HH

#include "CommandLineParser.hh"

namespace openmsx {

class MSXEventRecorder;
class MSXEventReplayer;

class MSXEventRecorderReplayerCLI
{
public:
	explicit MSXEventRecorderReplayerCLI(CommandLineParser& cmdLineParser);
	~MSXEventRecorderReplayerCLI();

private:
	class RecordOption : public CLIOption {
	public:
		RecordOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
		std::auto_ptr<MSXEventRecorder> eventRecorder;
	} recordOption;

	class ReplayOption : public CLIOption {
	public:
		ReplayOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
		std::auto_ptr<MSXEventReplayer> eventReplayer;
	} replayOption;
};

} // namespace openmsx

#endif
