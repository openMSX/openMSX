#ifndef PROXYSETTING_HH
#define PROXYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class Reactor;

class ProxySetting final : public BaseSetting
{
public:
	ProxySetting(Reactor& reactor, const TclObject& name);

	void setValue(const TclObject& value) override;
	[[nodiscard]] std::string_view getTypeString() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] const TclObject& getValue() const override;
	[[nodiscard]] std::optional<TclObject> getOptionalValue() const override;
	[[nodiscard]] TclObject getDefaultValue() const override;
	[[nodiscard]] TclObject getRestoreValue() const override;
	void setValueDirect(const TclObject& value) override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	[[nodiscard]] bool needLoadSave() const override;
	[[nodiscard]] bool needTransfer() const override;
	void setDontSaveValue(const TclObject& dontSaveValue) override;
	void additionalInfo(TclObject& result) const override;

private:
	[[nodiscard]]       BaseSetting* getSetting();
	[[nodiscard]] const BaseSetting* getSetting() const;

private:
	Reactor& reactor;
};

} // namespace openmsx

#endif
