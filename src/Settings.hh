// $Id$

#ifndef __SETTING_HH__
#define __SETTING_HH__

#include "Command.hh"
#include <map>

/*
TODO: A way to inform listeners of a setting update.

Listen to one or multiple settings?
One may be uncomfortable, because inner classes in C++ don't automatically
have a reference to the outer class.
But multiple bring the problem of identifying the source.

Identifying source:
- by name (string)
- by pointer
- by ID (to be added)


*/

/** Abstract base class for Settings.
  */
class Setting
{
public:

	/** Get the name of this setting.
	  */
	const std::string &getName() { return name; }

	/** Get a description of this setting that can be presented to the user.
	  */
	const std::string &getDescription() { return description; }

	/** Get the current value of this setting in a string format that can be
	  * presented to the user.
	  */
	virtual std::string getValueString() = 0;

	/** Change the value of this setting by parsing the given string.
	  * @param valueString The new value for this setting, in string format.
	  * @throw CommandException If the valueString is invalid.
	  */
	virtual void setValueString(const std::string &valueString) = 0;

	/** Get a string describing the value type to the user.
	  */
	const std::string &getTypeString() { return type; }

protected:

	/** Create a new Setting.
	  */
	Setting(const std::string &name, const std::string &description);

	/** Destructor.
	  */
	virtual ~Setting();

	/** A description of the type of this setting's value that can be
	  * presented to the user.
	  */
	std::string type;

private:

	/** The name of this setting.
	  */
	std::string name;

	/** A description of this setting that can be presented to the user.
	  */
	std::string description;

};

#if 0

/** A Setting with a boolean value.
  */
class BooleanSetting
{
public:

	/** Create a new BooleanSetting.
	  */
	BooleanSetting(value, enabled = false);

	/** Get the current value of this setting.
	  */
	bool getValue() { return value; }

protected:

private:

	bool value;

};

#endif

/** A Setting with an integer value.
  */
class IntegerSetting: public Setting
{
public:

	/** Create a new IntegerSetting.
	  */
	IntegerSetting(
		const std::string &name, const std::string &description,
		int initialValue, int minValue, int maxValue );

	/** Get the current value of this setting.
	  */
	int getValue() { return value; }

	// Implementation of Setting interface:
	std::string getValueString();
	void setValueString(const std::string &valueString);

protected:

private:

	int value;
	int minValue;
	int maxValue;

};

/** Manages all settings.
  */
class SettingsManager
{
private:
	std::map<const std::string, Setting *> settingsMap;

public:

	/** Get singleton instance.
	  */
	static SettingsManager *getInstance() {
		static SettingsManager *instance = NULL;
		if (!instance) instance = new SettingsManager();
		return instance;
	}

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting *getByName(const std::string &name) {
		std::map<std::string, Setting *>::const_iterator it =
			settingsMap.find(name);
		return it == settingsMap.end() ? NULL : it->second;
	}

	void registerSetting(const std::string &name, Setting *setting) {
		settingsMap[name] = setting;
	}

private:

	SettingsManager();

	class SetCommand : public Command {
		public:
			SetCommand(SettingsManager *manager);
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help   (const std::vector<std::string> &tokens);
			virtual void tabCompletion(std::vector<std::string> &tokens);
		private:
			SettingsManager *manager;
	};
	friend class SetCommand;
	SetCommand setCommand;

};

#endif //__SETTING_HH__

