// $Id$

#ifndef __SETTINGSMANAGER_HH__
#define __SETTINGSMANAGER_HH__

#include "Settings.hh"
#include "Command.hh"
#include <cassert>
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

namespace openmsx {

/** Manages all settings.
  */
class SettingsManager
{
private:
	map<string, SettingNode*> settingsMap;

public:

	/** Get singleton instance.
	  */
	static SettingsManager *instance() {
		static SettingsManager oneInstance;
		return &oneInstance;
	}

	/** Get a setting by specifying its name.
	  * @return The SettingLeafNode with the given name,
	  *   or NULL if there is no such SettingLeafNode.
	  */
	SettingLeafNode *getByName(const string &name) const {
		map<string, SettingNode *>::const_iterator it =
			settingsMap.find(name);
		// TODO: The cast is valid because currently all nodes are leaves.
		//       In the future this will no longer be the case.
		return it == settingsMap.end()
			? NULL
			: static_cast<SettingLeafNode *>(it->second);
	}

	void registerSetting(const string &name, SettingNode *setting) {
		assert(settingsMap.find(name) == settingsMap.end());
		settingsMap[name] = setting;
	}

	void unregisterSetting(const string &name) {
		settingsMap.erase(name);
	}

private:
	SettingsManager();
	~SettingsManager();

	template <typename T>
	void getSettingNames(string& result) const;

	template <typename T>
	void getSettingNames(set<string>& result) const;
	
	template <typename T>
	T* getByName(const string& cmd, const string& name) const;

	class SetCommand : public Command {
	public:
		SetCommand(SettingsManager *manager);
		virtual string execute(const vector<string> &tokens)
			throw (CommandException);
		virtual string help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	private:
		SettingsManager *manager;
	} setCommand;
	friend class SetCommand;

	class ToggleCommand : public Command {
	public:
		ToggleCommand(SettingsManager *manager);
		virtual string execute(const vector<string> &tokens)
			throw (CommandException);
		virtual string help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	private:
		SettingsManager *manager;
	} toggleCommand;
	friend class ToggleCommand;

	class IncrCommand : public Command {
	public:
		IncrCommand(SettingsManager *manager);
		virtual string execute(const vector<string> &tokens)
			throw (CommandException);
		virtual string help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	private:
		SettingsManager *manager;
	} incrCommand;
	friend class IncrCommand;

	class DecrCommand : public Command {
	public:
		DecrCommand(SettingsManager *manager);
		virtual string execute(const vector<string> &tokens)
			throw (CommandException);
		virtual string help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	private:
		SettingsManager *manager;
	} decrCommand;
	friend class DecrCommand;
};

} // namespace openmsx

#endif //__SETTINGSMANAGER_HH__
