// $Id$

#ifndef __SETTINGS_HH__
#define __SETTINGS_HH__

#include "Command.hh"
#include <map>
#include <cassert>


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
	const std::string &getName() const { return name; }

	/** Get a description of this setting that can be presented to the user.
	  */
	const std::string &getDescription() const { return description; }

	/** Get the current value of this setting in a string format that can be
	  * presented to the user.
	  */
	virtual std::string getValueString() const = 0;

	/** Change the value of this setting by parsing the given string.
	  * @param valueString The new value for this setting, in string format.
	  * @throw CommandException If the valueString is invalid.
	  */
	virtual void setValueString(const std::string &valueString) = 0;

	/** Get a string describing the value type to the user.
	  */
	const std::string &getTypeString() const { return type; }

	/** Complete a partly typed value
	  */
	virtual void tabCompletion(std::vector<std::string> &tokens) const {}

protected:
	Setting(const std::string &name, const std::string &description);

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


/** A Setting with an integer value.
  */
class IntegerSetting: public Setting
{
public:
	IntegerSetting(
		const std::string &name, const std::string &description,
		int initialValue, int minValue, int maxValue);

	/** Get the current value of this setting.
	  */
	int getValue() const { return value; }

	// Implementation of Setting interface:
	virtual std::string getValueString() const;
	virtual void setValueString(const std::string &valueString);
	virtual void setValueInt(int newValue);
	void setRange (const int minvalue,const int maxvalue);
	
protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 *                 contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(int newValue) {
		return true;
	}
		
	int value;
	int minValue;
	int maxValue;
};


/** A Setting with a string value out of a finite set.
  */
template <class T>
class EnumSetting : public Setting
{
public:
	EnumSetting(
		const std::string &name, const std::string &description,
		const T &initialValue,
		const std::map<std::string, T> &map);

	/** Get the current value of this setting.
	  */
	T getValue() const { return value; }
	
	/** Set the current value of this setting.
	  * @param value The new value.
	  */
	void setValue(T value);

	// Implementation of Setting interface:
	virtual std::string getValueString() const;
	virtual void setValueString(const std::string &valueString);
	virtual void tabCompletion(std::vector<std::string> &tokens) const;

protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 *                 contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(T newValue) {
		return true;
	}

	T value;
	typedef typename
	        std::map<std::string, T>::const_iterator MapIterator;
	const std::map<std::string, T> map;
};


/** A Setting with a boolean value.
  */
class BooleanSetting : public EnumSetting<bool>
{
public:
	BooleanSetting(
		const std::string &name, const std::string &description,
		bool initialValue = false);

private:
	static std::map<std::string, bool> &getMap();
};


/** A Setting with a string value.
  */
class StringSetting : public Setting
{
public:
	StringSetting(const std::string &name, const std::string &description,
	              const std::string &initialValue);

	// Implementation of Setting interface:
	virtual std::string getValueString() const;
	virtual void setValueString(const std::string &valueString);

protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 *                 contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(const std::string &newValue) {
		return true;
	}

	std::string value;
};


class File;
class FileContext;

/** A Setting with a filename value.
  */
class FilenameSetting : public StringSetting
{
public:
	FilenameSetting(const std::string &name,
	                const std::string &description,
	                const std::string &initialValue);

	// Implementation of Setting interface:
	virtual void tabCompletion(std::vector<std::string> &tokens) const;
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
	static SettingsManager *instance() {
		static SettingsManager oneInstance;
		return &oneInstance;
	}

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting *getByName(const std::string &name) const {
		std::map<std::string, Setting *>::const_iterator it =
			settingsMap.find(name);
		return it == settingsMap.end() ? NULL : it->second;
	}

	void registerSetting(const std::string &name, Setting *setting) {
		assert(settingsMap.find(name) == settingsMap.end());
		settingsMap[name] = setting;
	}
	void unregisterSetting(const std::string &name) {
		settingsMap.erase(name);
	}

private:
	SettingsManager();
	~SettingsManager();

	class SetCommand : public Command {
	public:
		SetCommand(SettingsManager *manager);
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens) const;
		virtual void tabCompletion(std::vector<std::string> &tokens) const;
	private:
		SettingsManager *manager;
	};
	friend class SetCommand;
	SetCommand setCommand;

	class ToggleCommand : public Command {
	public:
		ToggleCommand(SettingsManager *manager);
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens) const;
		virtual void tabCompletion(std::vector<std::string> &tokens) const;
	private:
		SettingsManager *manager;
	};
	friend class ToggleCommand;
	ToggleCommand toggleCommand;
};

#endif //__SETTINGS_HH__
