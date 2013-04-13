// $Id$

#ifndef SETTING_HH
#define SETTING_HH

#include "Subject.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <vector>

namespace openmsx {

class CommandController;
class GlobalCommandController;
class Interpreter;
class XMLElement;
class TclObject;

class Setting : public Subject<Setting>, private noncopyable
{
public:
	enum SaveSetting {
		SAVE,          //    save,    transfer
		DONT_SAVE,     //    save, no-transfer
		DONT_TRANSFER, // no-save, no-transfer
	};

	virtual ~Setting();

	/** Returns a string describing the setting type (integer, string, ..)
	  */
	virtual string_ref getTypeString() const = 0;

	/** Get the name of this setting.
	  */
	const std::string& getName() const;

	/** Get a description of this setting that can be presented to the user.
	  */
	virtual std::string getDescription() const;

	/** Change the value of this setting by parsing the given string.
	  * @param valueString The new value for this setting, in string format.
	  * @throw CommandException If the valueString is invalid.
	  */
	void changeValueString(const std::string& valueString);

	/** Get the current value of this setting in a string format that can be
	  * presented to the user.
	  */
	virtual std::string getValueString() const = 0;

	/** Get the default value of this setting.
	  */
	virtual std::string getDefaultValueString() const = 0;

	/** Get the value that will be set after a Tcl 'unset' command.
	  */
	virtual std::string getRestoreValueString() const = 0;

	/** Similar to changeValueString(), but doesn't trigger Tcl traces.
	  * Should only be used by Interpreter class.
	  */
	virtual void setValueStringDirect(const std::string& valueString) = 0;

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
	virtual bool needLoadSave() const;

	/** Needs this setting to be transfered on reverse.
	  */
	virtual bool needTransfer() const;

	/** This value will never end up in the settings.xml file
	 */
	virtual void setDontSaveValue(const std::string& dontSaveValue);

	/** Synchronize the setting with the SettingsConfig. Should be called
	  * just before saving the setting or just before the setting is
	  * deleted.
	  */
	void sync(XMLElement& config) const;

	/** For SettingInfo
	  */
	void info(TclObject& result) const;

	CommandController& getCommandController() const;
	GlobalCommandController& getGlobalCommandController() const;
	Interpreter& getInterpreter() const;

	// helper method for info()
	virtual void additionalInfo(TclObject& result) const = 0;

protected:
	Setting(CommandController& commandController, string_ref name,
	        string_ref description, SaveSetting save);

	/** Notify all listeners of a change to this setting's value.
	  * Still needed, because it also informs the CliComm stuff that there
	  * was an update, next to calling the normal notify implementation of
	  * the Subject class.
	  */
	void notify() const;

	void notifyPropertyChange() const;

private:
	CommandController& commandController;

	/** The name of this setting.
	  */
	const std::string name;

	/** A description of this setting that can be presented to the user.
	  */
	const std::string description;

	/** see setDontSaveValue()
	 */
	std::string dontSaveValue;

	/** need to be saved flag
	 */
	const SaveSetting save;
};

} // namespace openmsx

#endif
