#ifndef PROXYSETTING_HH
#define PROXYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class Reactor;

class ProxySetting final : public BaseSetting
{
public:
	ProxySetting(Reactor& reactor, string_ref name);

	void setString(const std::string& value) override;
	string_ref getTypeString() const override;
	std::string getDescription() const override;
	std::string getString() const override;
	std::string getDefaultValue() const override;
	std::string getRestoreValue() const override;
	void setStringDirect(const std::string& value) override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	bool needLoadSave() const override;
	bool needTransfer() const override;
	void setDontSaveValue(const std::string& dontSaveValue) override;
	void additionalInfo(TclObject& result) const override;

private:
	BaseSetting* getSetting();
	const BaseSetting* getSetting() const;

	Reactor& reactor;
};

} // namespace openmsx

#endif
