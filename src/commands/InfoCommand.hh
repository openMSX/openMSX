// $Id$

#ifndef __INFOCOMMAND_HH__
#define __INFOCOMMAND_HH__

#include <map>
#include "Command.hh"
#include "InfoTopic.hh"

using std::map;

namespace openmsx {

class InfoCommand : public Command
{
public:
	static InfoCommand& instance();
	void registerTopic(const string& name, const InfoTopic* topic);
	void unregisterTopic(const string& name, const InfoTopic* topic);
	
	// Command
	virtual void execute(const vector<string>& tokens, CommandResult& result)
		throw(CommandException);
	virtual string help(const vector<string>& tokens) const
		throw(CommandException);
	virtual void tabCompletion(vector<string>& tokens) const
		throw();

private:
	InfoCommand();
	virtual ~InfoCommand();

	map<string, const InfoTopic*> infoTopics;


	class VersionInfo : public InfoTopic {
	public:
		virtual void execute(const vector<string>& tokens,
		                     CommandResult& result) const
			throw();
		virtual string help   (const vector<string>& tokens) const
			throw();
	} versionInfo;
};

} // namespace openmsx

#endif
