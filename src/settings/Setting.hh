// $Id$

#ifndef SETTINGNODE_HH
#define SETTINGNODE_HH

#include "Subject.hh"
#include <string>
#include <vector>

namespace openmsx {

class CommandController;
class XMLElement;
class TclObject;

class Setting : public Subject<Setting>
{
public:
	enum SaveSetting {
		SAVE,
		DONT_SAVE
	};

	/** Returns a string describing the setting type (integer, string, ..)
	  */
	virtual std::string getTypeString() const = 0;

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

	/** Get the default value of this setting.
	  */
	virtual std::string getDefaultValueString() const = 0;

	/** Change the value of this setting by parsing the given string.
	  * @param valueString The new value for this setting, in string format.
	  * @throw CommandException If the valueString is invalid.
	  */
	virtual void setValueString(const std::string& valueString) = 0;

	/** Restore the default value.
	 */
	virtual void restoreDefault() = 0;

	/** Checks whether the current value is the default value.
	 */
	virtual bool hasDefaultValue() const = 0;

	/** Complete a partly typed value.
	  * Default implementation does not complete anything,
	  * subclasses can override this to complete according to their
	  * specific value type.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;

	/** Needs this setting to be loaded or saved
	  */
	bool needLoadSave() const;

	/** Synchronize the setting with the SettingsConfig. Should be called
	  * just before saving the setting or just before the setting is
	  * deleted.
	  */
	void sync(XMLElement& config) const;

	/** For SettingInfo
	  */
	void info(TclObject& result) const;

	CommandController& getCommandController() const;

protected:
	Setting(CommandController& commandController, const std::string& name,
	        const std::string& description, SaveSetting save);
	virtual ~Setting();

	/** Notify all listeners of a change to this setting's value.
	  * Still needed, because it also informs the CliComm stuff that there
	  * was an update, next to calling the normal notify implementation of
	  * the Subject class.
	  */
	void notify() const;

	// helper method for info()
	virtual void additionalInfo(TclObject& result) const = 0;

private:
	CommandController& commandController;

	/** The name of this setting.
	  */
	std::string name;

	/** A description of this setting that can be presented to the user.
	  */
	std::string description;

	/** need to be saved flag
	 */
	bool save;
};

} // namespace openmsx

#endif
