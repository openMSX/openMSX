// $Id$

#ifndef __SETTINGNODE_HH__
#define __SETTINGNODE_HH__

#include <string>
#include <vector>
#include <list>

using std::string;
using std::vector;
using std::list;

namespace openmsx {

class SettingListener;

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

	/** Restore the default value.
	 */
	virtual void restoreDefault() = 0;
	
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

} // namespace openmsx

#endif //__SETTINGNODE_HH__
