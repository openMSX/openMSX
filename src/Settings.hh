// $Id$

#ifndef __SETTINGS_HH__
#define __SETTINGS_HH__

#include "Command.hh"
#include "CommandController.hh"
#include <cassert>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>

using std::map;
using std::set;
using std::string;
using std::vector;
using std::list;

/*
TODO: Reduce code duplication.
It is possible to put a lot of the common code in templates:
- give Setting base class a value type
- make OrderedSetting/OrdinalSetting/NumberSetting/ScalarSetting
  base class for IntegerSetting and FloatSetting

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

class Setting;

class SettingListener
{
public:
	virtual void notify(Setting *setting) = 0;
};


/** Abstract base class for Settings.
  */
class Setting
{
public:
	/** Get the name of this setting.
	  */
	const string &getName() const { return name; }

	/** Get a description of this setting that can be presented to the user.
	  */
	const string &getDescription() const { return description; }

	/** Get the current value of this setting in a string format that can be
	  * presented to the user.
	  */
	virtual string getValueString() const = 0;

	/** Change the value of this setting by parsing the given string.
	  * @param valueString The new value for this setting, in string format.
	  * @throw CommandException If the valueString is invalid.
	  */
	virtual void setValueString(const string &valueString) = 0;

	/** Get a string describing the value type to the user.
	  */
	const string &getTypeString() const { return type; }

	/** Complete a partly typed value
	  */
	virtual void tabCompletion(vector<string> &tokens) const {}

	void registerListener(SettingListener *listener);
	void unregisterListener(SettingListener *listener);

protected:
	Setting(const string &name, const string &description);

	virtual ~Setting();

	/** A description of the type of this setting's value that can be
	  * presented to the user.
	  */
	string type;

	void notifyAll();

private:
	/** The name of this setting.
	  */
	string name;

	/** A description of this setting that can be presented to the user.
	  */
	string description;

	list<SettingListener*> listeners;
};


/** A Setting with an integer value.
  */
class IntegerSetting: public Setting
{
public:
	IntegerSetting(
		const string &name, const string &description,
		int initialValue, int minValue, int maxValue);

	/** Get the current value of this setting.
	  */
	int getValue() const { return value; }

	/** Change the current value of this setting.
	  * The value will be clipped to the allowed range if necessary.
	  * @param newValue The new value.
	  */
	virtual void setValueInt(int newValue);

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(int minValue, int maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);


protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 * contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(int newValue) {
		return true;
	}

	int value;
	int minValue;
	int maxValue;
};


/** A Setting with a floating point value.
  */
class FloatSetting: public Setting
{
public:
	FloatSetting(
		const string &name, const string &description,
		float initialValue, float minValue, float maxValue);

	/** Get the current value of this setting.
	  */
	float getValue() const { return value; }

	/** Change the current value of this setting.
	  * The value will be clipped to the allowed range if necessary.
	  * @param newValue The new value.
	  */
	virtual void setValueFloat(float newValue);

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(float minValue, float maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);

protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 * contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(float newValue) {
		return true;
	}

	float value;
	float minValue;
	float maxValue;
};


/** Associates pairs of an a integer and a string.
  * Used internally by EnumSetting to associate enum constants with strings.
  */
class IntStringMap
{
public:
	typedef const map<const string, int> BaseMap;

	IntStringMap(BaseMap *map);
	~IntStringMap();

	/** Gets the string associated with a given integer.
	  */
	const string &lookupInt(int n) const;

	/** Gets the string associated with a given string.
	  */
	int lookupString(const string &s) const;

	/** Get the list of all strings, separated by commas.
	  */
	string getSummary() const;

	/** Create a set containing all the strings in this association.
	  * The caller becomes the owner of the newly created set object.
	  */
	set<string> *createStringSet() const;

private:
	typedef map<const string, int>::const_iterator MapIterator;
	BaseMap *stringToInt;
};


/** A Setting with a string value out of a finite set.
  * The type parameter can be anything that can be converted from and to
  * an integer.
  */
template <class T>
class EnumSetting : public Setting
{
public:
	EnumSetting(
		const string &name, const string &description,
		const T &initialValue,
		const map<const string, T> &map
	) : Setting(name, description)
	  , value(initialValue)
	  , intStringMap(convertMap(map))
	{
		type = intStringMap.getSummary();
	}

	/** Get the current value of this setting.
	  */
	T getValue() const { return value; }

	/** Set the current value of this setting.
	  * @param newValue The new value.
	  */
	void setValue(T newValue) {
		// TODO: Move code to Setting.
		if (value != newValue) {
			if (checkUpdate(newValue)) {
				value = newValue;
				notifyAll();
			}
		}
	}

	// Implementation of Setting interface:
	virtual string getValueString() const {
		return intStringMap.lookupInt(static_cast<int>(value));
	}

	virtual void setValueString(const string &valueString) {
		setValue(static_cast<T>(intStringMap.lookupString(valueString)));
	}

	virtual void tabCompletion(vector<string> &tokens) const {
		set<string> *stringSet = intStringMap.createStringSet();
		CommandController::completeString(tokens, *stringSet);
		delete stringSet;
	}

protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 * contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(T newValue) {
		return true;
	}

	T value;
	IntStringMap intStringMap;

private:
	static map<const string, int> *convertMap(
		const map<const string, T> &map_
	) {
		map<const string, int> *ret = new map<const string, int>();
		typename map<const string, T>::const_iterator it;
		for (it = map_.begin() ; it != map_.end() ; it++) {
			(*ret)[it->first] = static_cast<int>(it->second);
		}
		return ret;
	}
};


/** A Setting with a boolean value.
  */
class BooleanSetting : public EnumSetting<bool>
{
public:
	BooleanSetting(
		const string &name, const string &description,
		bool initialValue = false);

private:
	static const map<const string, bool> &getMap();
};


/** A Setting with a string value.
  */
class StringSetting : public Setting
{
public:
	StringSetting(const string &name, const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);

protected:
	/**
	 * Called just before this setting is assigned a new value
	 * @param newValue The new value, the variable value still
	 *                 contains the old value
	 * @return Only when the result is true the new value is assigned
	 */
	virtual bool checkUpdate(const string &newValue) {
		return true;
	}

	string value;
};


class File;
class FileContext;

/** A Setting with a filename value.
  */
class FilenameSetting : public StringSetting
{
public:
	FilenameSetting(const string &name,
		const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual void tabCompletion(vector<string> &tokens) const;
};


/** Manages all settings.
  */
class SettingsManager
{
private:
	map<const string, Setting *> settingsMap;

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
	Setting *getByName(const string &name) const {
		map<string, Setting *>::const_iterator it =
			settingsMap.find(name);
		return it == settingsMap.end() ? NULL : it->second;
	}

	void registerSetting(const string &name, Setting *setting) {
		assert(settingsMap.find(name) == settingsMap.end());
		settingsMap[name] = setting;
	}
	void unregisterSetting(const string &name) {
		settingsMap.erase(name);
	}

private:
	SettingsManager();
	~SettingsManager();

	class SetCommand : public Command {
	public:
		SetCommand(SettingsManager *manager);
		virtual void execute(const vector<string> &tokens);
		virtual void help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	private:
		SettingsManager *manager;
	};
	friend class SetCommand;
	SetCommand setCommand;

	class ToggleCommand : public Command {
	public:
		ToggleCommand(SettingsManager *manager);
		virtual void execute(const vector<string> &tokens);
		virtual void help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	private:
		SettingsManager *manager;
	};
	friend class ToggleCommand;
	ToggleCommand toggleCommand;
};

#endif //__SETTINGS_HH__
