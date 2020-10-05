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
	std::string_view getTypeString() const override;
	std::string_view getDescription() const override;
	const TclObject& getValue() const override;
	TclObject getDefaultValue() const override;
	TclObject getRestoreValue() const override;
	void setValueDirect(const TclObject& value) override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	bool needLoadSave() const override;
	bool needTransfer() const override;
	void setDontSaveValue(const TclObject& dontSaveValue) override;
	void additionalInfo(TclObject& result) const override;

private:
	BaseSetting* getSetting();
	const BaseSetting* getSetting() const;

	Reactor& reactor;
};

} // namespace openmsx

#endif
