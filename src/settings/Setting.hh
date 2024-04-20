#ifndef SETTING_HH
#define SETTING_HH

#include "Subject.hh"
#include "TclObject.hh"
#include "static_string_view.hh"
#include "strCat.hh"
#include <cassert>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace openmsx {

class CommandController;
class GlobalCommandController;
class Interpreter;

class BaseSetting
{
protected:
	explicit BaseSetting(std::string_view name);
	explicit BaseSetting(TclObject name);
	~BaseSetting() = default;

public:
	/** Get the name of this setting.
	  * For global settings 'fullName' and 'baseName' are the same. For
	  * machine specific settings 'fullName' is the fully qualified name,
	  * and 'baseName' is the name without machine-prefix. For example:
	  *   fullName = "::machine1::PSG_volume"
	  *   baseName = "PSG_volume"
	  */
	[[nodiscard]] const TclObject& getFullNameObj() const { return fullName; }
	[[nodiscard]] const TclObject& getBaseNameObj() const { return baseName; }
	[[nodiscard]] std::string_view getFullName()    const { return fullName.getString(); }
	[[nodiscard]] std::string_view getBaseName()    const { return baseName.getString(); }

	/** Set a machine specific prefix.
	 */
	void setPrefix(std::string_view prefix) {
		assert(prefix.starts_with("::"));
		fullName = tmpStrCat(prefix, getBaseName());
	}

	/** For SettingInfo
	  */
	void info(TclObject& result) const;

	/// pure virtual methods ///

	/** Get a description of this setting that can be presented to the user.
	  */
	[[nodiscard]] virtual std::string_view getDescription() const = 0;

	/** Returns a string describing the setting type (integer, string, ..)
	  * Could be used in a GUI to pick an appropriate setting widget.
	  */
	[[nodiscard]] virtual std::string_view getTypeString() const = 0;

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
	[[nodiscard]] virtual const TclObject& getValue() const = 0;

	/** Like getValue(), but in case of error returns an empty optional
	  * instead of throwing an exception.
	  */
	[[nodiscard]] virtual std::optional<TclObject> getOptionalValue() const = 0;

	/** Get the default value of this setting.
	  * This is the initial value of the setting. Default values don't
	  * get saved in 'settings.xml'.
	  * This is also the value used for a Tcl 'unset' command.
	  */
	[[nodiscard]] virtual TclObject getDefaultValue() const = 0;

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
	[[nodiscard]] virtual bool needLoadSave() const = 0;

	/** Does this setting need to be transfered on reverse.
	  */
	[[nodiscard]] virtual bool needTransfer() const = 0;

private:
	      TclObject fullName;
	const TclObject baseName;
};


class Setting : public BaseSetting, public Subject<Setting>
{
public:
	enum SaveSetting {
		SAVE,          //    save,    transfer
		DONT_SAVE,     // no-save,    transfer
		DONT_TRANSFER, // no-save, no-transfer
	};

	Setting(const Setting&) = delete;
	Setting(Setting&&) = delete;
	Setting& operator=(const Setting&) = delete;
	Setting& operator=(Setting&&) = delete;
	virtual ~Setting();

	/** Gets the current value of this setting as a TclObject.
	  */
	[[nodiscard]] const TclObject& getValue() const final { return value; }
	[[nodiscard]] std::optional<TclObject> getOptionalValue() const final { return value; }

	/** Set value-check-callback.
	 * The callback is called on each change of this settings value.
	 * The callback has the possibility to
	 *  - change the value (modify the parameter)
	 *  - disallow the change (throw an exception)
	 * The callback is only executed on each value change, even if the
	 * new value is the same as the current value. However the callback
	 * is not immediately executed once it's set (via this method).
	 */
	void setChecker(std::function<void(TclObject&)> checkFunc_) {
		checkFunc = std::move(checkFunc_);
	}

	// BaseSetting
	void setValue(const TclObject& newValue) final;
	[[nodiscard]] std::string_view getDescription() const final;
	[[nodiscard]] TclObject getDefaultValue() const final { return defaultValue; }
	void setValueDirect(const TclObject& newValue) final;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	[[nodiscard]] bool needLoadSave() const final;
	void additionalInfo(TclObject& result) const override;
	[[nodiscard]] bool needTransfer() const final;

	// convenience functions
	[[nodiscard]] CommandController& getCommandController() const { return commandController; }
	[[nodiscard]] Interpreter& getInterpreter() const;

protected:
	Setting(CommandController& commandController,
	        std::string_view name, static_string_view description,
	        const TclObject& initialValue, SaveSetting save = SAVE);
	void init();
	void notifyPropertyChange() const;

private:
	[[nodiscard]] GlobalCommandController& getGlobalCommandController() const;
	void notify() const;

private:
	CommandController& commandController;
	const static_string_view description;
	std::function<void(TclObject&)> checkFunc;
	TclObject value; // TODO can we share the underlying Tcl var storage?
	const TclObject defaultValue;
	const SaveSetting save;
};

} // namespace openmsx

#endif
