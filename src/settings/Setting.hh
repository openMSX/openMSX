#ifndef SETTING_HH
#define SETTING_HH

#include "Subject.hh"
#include "TclObject.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <functional>
#include <vector>

namespace openmsx {

class CommandController;
class GlobalCommandController;
class Interpreter;

class BaseSetting : private noncopyable
{
protected:
	BaseSetting(string_ref name);
	~BaseSetting() {}

public:
	/** Get the name of this setting.
	  */
	const std::string& getName() const;

	/** For SettingInfo
	  */
	void info(TclObject& result) const;

	/// pure virtual methods ///

	/** Get a description of this setting that can be presented to the user.
	  */
	virtual std::string getDescription() const = 0;

	/** Returns a string describing the setting type (integer, string, ..)
	  * Could be used in a GUI to pick an appropriate setting widget.
	  */
	virtual string_ref getTypeString() const = 0;

	/** Helper method for info().
	 */
	virtual void additionalInfo(TclObject& result) const = 0;

	/** Complete a partly typed value.
	  * Default implementation does not complete anything,
	  * subclasses can override this to complete according to their
	  * specific value type.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;

	/** Get the current value of this setting in a string format that can be
	  * presented to the user.
	  */
	virtual std::string getString() const = 0;

	/** Get the default value of this setting.
	  * This is the initial value of the setting. Default values don't
	  * get saved in 'settings.xml'.
	  */
	virtual std::string getDefaultValue() const = 0;

	/** Get the value that will be set after a Tcl 'unset' command.
	  * Usually this is the same as the default value. Though one
	  * exception is 'renderer', see comments in RendererFactory.cc.
	  */
	virtual std::string getRestoreValue() const = 0;

	/** Change the value of this setting to the given value.
	  * This method will trigger Tcl traces.
	  * This value still passes via the 'checker-callback' (see below),
	  * so the value may be adjusted. Or in case of an invalid value
	  * this method may throw.
	  */
	virtual void setString(const std::string& value) = 0;

	/** Similar to setString(), but doesn't trigger Tcl traces.
	  * Like setString(), the given value may be adjusted or rejected.
	  * Should only be used by the Interpreter class.
	  */
	virtual void setStringDirect(const std::string& value) = 0;

	/** Does this setting need to be loaded or saved (settings.xml).
	  */
	virtual bool needLoadSave() const = 0;

	/** Does this setting need to be transfered on reverse.
	  */
	virtual bool needTransfer() const = 0;

	/** This value will never end up in the settings.xml file
	 */
	virtual void setDontSaveValue(const std::string& dontSaveValue) = 0;

private:
	/** The name of this setting. */
	const std::string name;
};


class Setting : public BaseSetting, public Subject<Setting>
{
public:
	enum SaveSetting {
		SAVE,          //    save,    transfer
		DONT_SAVE,     // no-save,    transfer
		DONT_TRANSFER, // no-save, no-transfer
	};

	virtual ~Setting();

	/** Gets the current value of this setting as a TclObject.
	  */
	const TclObject& getValue() const { return value; }

	/** Set restore value. See getDefaultValue() and getRestoreValue().
	  */
	void setRestoreValue(const std::string& value);

	/** Set value-check-callback.
	 * The callback is called on each change of this settings value.
	 * The callback has to posibility to
	 *  - change the value (modify the parameter)
	 *  - disallow the change (throw an exception)
	 * The callback is only executed on each value change, even if the
	 * new value is the same as the current value. However the callback
	 * is not immediately executed once it's set (via this method).
	 */
	void setChecker(std::function<void(TclObject&)> checkFunc);

	// BaseSetting
	virtual void setString(const std::string& value);
	virtual std::string getDescription() const;
	virtual std::string getString() const;
	virtual std::string getDefaultValue() const;
	virtual std::string getRestoreValue() const;
	virtual void setStringDirect(const std::string& value);
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
	virtual bool needLoadSave() const;
	virtual void additionalInfo(TclObject& result) const;
	virtual bool needTransfer() const;
	virtual void setDontSaveValue(const std::string& dontSaveValue);

	// convenience functions
	CommandController& getCommandController() const;
	Interpreter& getInterpreter() const;

protected:
	Setting(CommandController& commandController,
	        string_ref name, string_ref description,
	        const std::string& initialValue, SaveSetting save = SAVE);
	void notifyPropertyChange() const;

private:
	GlobalCommandController& getGlobalCommandController() const;
	void notify() const;

private:
	CommandController& commandController;
	const std::string description;
	std::function<void(TclObject&)> checkFunc;
	TclObject value; // TODO can we share the underlying Tcl var storage?
	const std::string defaultValue;
	std::string restoreValue;
	std::string dontSaveValue;
	const SaveSetting save;
};

} // namespace openmsx

#endif
