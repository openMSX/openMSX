#ifndef SETTING_HH
#define SETTING_HH

#include "Subject.hh"
#include "TclObject.hh"
#include "string_ref.hh"
#include <functional>
#include <vector>

namespace openmsx {

class CommandController;
class GlobalCommandController;
class Interpreter;

class BaseSetting
{
protected:
	BaseSetting(string_ref name);
	BaseSetting(const TclObject& name);
	~BaseSetting() {}

public:
	/** Get the name of this setting.
	  * For global settings 'fullName' and 'baseName' are the same. For
	  * machine specific settings 'fullName' is the fully qualified name,
	  * and 'baseName' is the name without machine-prefix. For example:
	  *   fullName = "::machine1::PSG_volume"
	  *   baseName = "PSG_volume"
	  */
	const TclObject& getFullNameObj() const { return fullName; }
	const TclObject& getBaseNameObj() const { return baseName; }
	const string_ref getFullName()    const { return fullName.getString(); }
	const string_ref getBaseName()    const { return baseName.getString(); }

	/** Set a machine specific prefix.
	 */
	void setPrefix(string_ref prefix) {
		assert(prefix.starts_with("::"));
		fullName.setString(prefix + getBaseName());
	}

	/** For SettingInfo
	  */
	void info(TclObject& result) const;

	/// pure virtual methods ///

	/** Get a description of this setting that can be presented to the user.
	  */
	virtual string_ref getDescription() const = 0;

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

	/** Get current value as a TclObject.
	 */
	virtual const TclObject& getValue() const = 0;

	/** Get the default value of this setting.
	  * This is the initial value of the setting. Default values don't
	  * get saved in 'settings.xml'.
	  */
	virtual TclObject getDefaultValue() const = 0;

	/** Get the value that will be set after a Tcl 'unset' command.
	  * Usually this is the same as the default value. Though one
	  * exception is 'renderer', see comments in RendererFactory.cc.
	  */
	virtual TclObject getRestoreValue() const = 0;

	/** Change the value of this setting to the given value.
	  * This method will trigger Tcl traces.
	  * This value still passes via the 'checker-callback' (see below),
	  * so the value may be adjusted. Or in case of an invalid value
	  * this method may throw.
	  */
	virtual void setValue(const TclObject& value) = 0;

	/** Similar to setValue(), but doesn't trigger Tcl traces.
	  * Like setValue(), the given value may be adjusted or rejected.
	  * Should only be used by the Interpreter class.
	  */
	virtual void setValueDirect(const TclObject& value) = 0;

	/** Does this setting need to be loaded or saved (settings.xml).
	  */
	virtual bool needLoadSave() const = 0;

	/** Does this setting need to be transfered on reverse.
	  */
	virtual bool needTransfer() const = 0;

	/** This value will never end up in the settings.xml file
	 */
	virtual void setDontSaveValue(const TclObject& dontSaveValue) = 0;

private:
	      TclObject fullName;
	const TclObject baseName;
};


class Setting : public BaseSetting, public Subject<Setting>
{
public:
	Setting(const Setting&) = delete;
	Setting& operator=(const Setting&) = delete;

	enum SaveSetting {
		SAVE,          //    save,    transfer
		DONT_SAVE,     // no-save,    transfer
		DONT_TRANSFER, // no-save, no-transfer
	};

	virtual ~Setting();

	/** Gets the current value of this setting as a TclObject.
	  */
	const TclObject& getValue() const final override { return value; }

	/** Set restore value. See getDefaultValue() and getRestoreValue().
	  */
	void setRestoreValue(const TclObject& newRestoreValue) {
		restoreValue = newRestoreValue;
	}

	/** Set value-check-callback.
	 * The callback is called on each change of this settings value.
	 * The callback has to posibility to
	 *  - change the value (modify the parameter)
	 *  - disallow the change (throw an exception)
	 * The callback is only executed on each value change, even if the
	 * new value is the same as the current value. However the callback
	 * is not immediately executed once it's set (via this method).
	 */
	void setChecker(std::function<void(TclObject&)> checkFunc_) {
		checkFunc = checkFunc_;
	}

	// BaseSetting
	void setValue(const TclObject& value) final override;
	string_ref getDescription() const final override;
	TclObject getDefaultValue() const final override { return defaultValue; }
	TclObject getRestoreValue() const final override { return restoreValue; }
	void setValueDirect(const TclObject& value) final override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	bool needLoadSave() const final override;
	void additionalInfo(TclObject& result) const override;
	bool needTransfer() const final override;
	void setDontSaveValue(const TclObject& dontSaveValue) final override;

	// convenience functions
	CommandController& getCommandController() const { return commandController; }
	Interpreter& getInterpreter() const;

protected:
	Setting(CommandController& commandController,
	        string_ref name, string_ref description,
	        const TclObject& initialValue, SaveSetting save = SAVE);
	void init();
	void notifyPropertyChange() const;

private:
	GlobalCommandController& getGlobalCommandController() const;
	void notify() const;

private:
	CommandController& commandController;
	const std::string description;
	std::function<void(TclObject&)> checkFunc;
	TclObject value; // TODO can we share the underlying Tcl var storage?
	const TclObject defaultValue;
	TclObject restoreValue;
	TclObject dontSaveValue;
	const SaveSetting save;
};

} // namespace openmsx

#endif
