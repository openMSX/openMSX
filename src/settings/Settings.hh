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
- make OrderedSetting/OrdinalSetting/NumberSetting/ScalarSetting
  base class for IntegerSetting and FloatSetting
*/

class SettingLeafNode;

/** Interface for listening to setting changes.
  */
class SettingListener
{
public:
	/** Informs a listener of a change in a setting it subscribed to.
	  * @param setting The setting of which the value has changed.
	  */
	virtual void update(const SettingLeafNode *setting) = 0;
};

/** Node in the hierarchical setting namespace.
  */
class SettingNode
{
public:
	/** Get the name of this setting.
	  */
	const string &getName() const { return name; }

	/** Get a description of this setting that can be presented to the user.
	  */
	const string &getDescription() const { return description; }

	/** Complete a partly typed setting name or value.
	  */
	virtual void tabCompletion(vector<string> &tokens) const = 0;

	// TODO: Does it make sense to listen to inner (group) nodes?
	//       If so, move listener mechanism to this class.

protected:
	SettingNode(const string &name, const string &description);

	virtual ~SettingNode();

private:
	/** The name of this setting.
	  */
	string name;

	/** A description of this setting that can be presented to the user.
	  */
	string description;
};

/** Leaf in the hierarchical setting namespace.
  * A leaf contains a single setting.
  */
class SettingLeafNode: public SettingNode
{
public:
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

	/** Complete a partly typed value.
	  * Default implementation does not complete anything,
	  * subclasses can override this to complete according to their
	  * specific value type.
	  */
	virtual void tabCompletion(vector<string> &tokens) const { }

	/** Subscribes a listener to changes of this setting.
	  */
	void addListener(SettingListener *listener);

	/** Unsubscribes a listener to changes of this setting.
	  */
	void removeListener(SettingListener *listener);

protected:
	SettingLeafNode(const string &name, const string &description);

	virtual ~SettingLeafNode();

	/** Notify all listeners of a change to this setting's value.
	  */
	void notify() const;

	/** A description of the type of this setting's value that can be
	  * presented to the user.
	  */
	string type;

private:
	list<SettingListener *> listeners;
};

/** Abstract base class for Settings.
  */
template <class ValueType>
class Setting: public SettingLeafNode
{
public:
	/** Gets the current value of this setting.
	  */
	const ValueType &getValue() const { return value; }

	/** Changes the current value of this setting.
	  * If the given value is invalid, it will be mapped to the closest
	  * valid value.
	  * If that is not appropriate or possible,
	  * the value of this setting will not change.
	  * The implementation of setValueString should use this method
	  * to set the new value.
	  * @param newValue The new value.
	  */
	virtual void setValue(const ValueType &newValue) {
		if (newValue != value) {
			value = newValue;
			notify();
		}
	}

protected:
	Setting(
		const string &name, const string &description,
		const ValueType &initialValue
		)
		: SettingLeafNode(name, description)
		, value(initialValue)
	{
	}

	virtual ~Setting() { }

	/** The current value of this setting.
	  */
	ValueType value;
};


/** A Setting with an integer value.
  */
class IntegerSetting: public Setting<int>
{
public:
	IntegerSetting(
		const string &name, const string &description,
		int initialValue, int minValue, int maxValue);

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(int minValue, int maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);
	virtual void setValue(const int &newValue);

protected:
	int minValue;
	int maxValue;
};

/** A Setting with a floating point value.
  */
class FloatSetting: public Setting<float>
{
public:
	FloatSetting(
		const string &name, const string &description,
		float initialValue, float minValue, float maxValue);

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(float minValue, float maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);
	virtual void setValue(const float &newValue);

protected:
	float minValue;
	float maxValue;
};

/** Associates pairs of an a integer and a string.
  * Used internally by EnumSetting to associate enum constants with strings.
  */
class IntStringMap
{
public:
	typedef const map<string, int> BaseMap;

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
	typedef map<string, int>::const_iterator MapIterator;
	BaseMap *stringToInt;
};


/** A Setting with a string value out of a finite set.
  * The type parameter can be anything that can be converted from and to
  * an integer.
  */
template <class ValueType>
class EnumSetting : public Setting<ValueType>
{
public:
	EnumSetting(
		const string &name, const string &description,
		const ValueType &initialValue,
		const map<string, ValueType> &map
	) : Setting<ValueType>(name, description, initialValue)
	  , intStringMap(convertMap(map))
	{
		type = intStringMap.getSummary();
	}

	// Implementation of Setting interface:
	virtual string getValueString() const {
		return intStringMap.lookupInt(static_cast<int>(value));
	}

	virtual void setValueString(const string &valueString) {
		setValue(
			static_cast<ValueType>(intStringMap.lookupString(valueString))
			);
	}

	virtual void tabCompletion(vector<string> &tokens) const {
		set<string> *stringSet = intStringMap.createStringSet();
		CommandController::completeString(tokens, *stringSet);
		delete stringSet;
	}

protected:
	IntStringMap intStringMap;

private:
	static map<string, int> *convertMap(
		const map<string, ValueType> &map_
	) {
		map<string, int> *ret = new map<string, int>();
		typename map<string, ValueType>::const_iterator it;
		for (it = map_.begin(); it != map_.end(); ++it) {
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
	static const map<string, bool> &getMap();
};


/** A Setting with a string value.
  */
class StringSetting : public Setting<string>
{
public:
	StringSetting(const string &name, const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);

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
	virtual void setValue(const string &newValue);
	virtual void tabCompletion(vector<string> &tokens) const;

protected:
	/** Used by subclass to check a new file name and/or contents.
	  * The default implementation accepts any file.
	  * @param filename The file path to an existing file.
	  * @return true to accept this file name; false to reject it.
	  */
	virtual bool checkFile(const string &filename);
};

/** Manages all settings.
  */
class SettingsManager
{
private:
	map<string, SettingNode *> settingsMap;

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
