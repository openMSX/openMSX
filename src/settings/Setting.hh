// $Id$

#ifndef SETTINGNODE_HH
#define SETTINGNODE_HH

#include <string>
#include <vector>

namespace openmsx {

class SettingListener;

class Setting 
{
public:
	enum SaveSetting {
		SAVE,
		DONT_SAVE,
	};

	/** Get the name of this setting.
	  */
	const std::string& getName() const { return name; }

	/** Get a description of this setting that can be presented to the user.
	  */
	const std::string& getDescription() const { return description; }

	/** Get the current value of this setting in a string format that can be
	  * presented to the user.
	  */
	virtual std::string getValueString() const = 0;

	/** Change the value of this setting by parsing the given string.
	  * @param valueString The new value for this setting, in string format.
	  * @throw CommandException If the valueString is invalid.
	  */
	virtual void setValueString(const std::string& valueString) = 0;

	/** Restore the default value.
	 */
	virtual void restoreDefault() = 0;
	
	/** Complete a partly typed value.
	  * Default implementation does not complete anything,
	  * subclasses can override this to complete according to their
	  * specific value type.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;

	/** Subscribes a listener to changes of this setting.
	  */
	void addListener(SettingListener* listener);

	/** Unsubscribes a listener to changes of this setting.
	  */
	void removeListener(SettingListener* listener);

protected:
	Setting(const std::string& name, const std::string& description);

	virtual ~Setting();

	/** Notify all listeners of a change to this setting's value.
	  */
	void notify() const;

private:
	/** The name of this setting.
	  */
	std::string name;

	/** A description of this setting that can be presented to the user.
	  */
	std::string description;

	/** Collection of all listeners
	 */
	typedef std::vector<SettingListener*> Listeners;
	Listeners listeners;
};

} // namespace openmsx

#endif
