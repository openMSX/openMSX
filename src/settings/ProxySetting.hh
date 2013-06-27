#ifndef PROXYSETTING_HH
#define PROXYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class Reactor;

class ProxySetting : public BaseSetting
{
public:
	ProxySetting(Reactor& reactor, string_ref name);

	virtual void setString(const std::string& value);
	virtual string_ref getTypeString() const;
	virtual std::string getDescription() const;
	virtual std::string getString() const;
	virtual std::string getDefaultValue() const;
	virtual std::string getRestoreValue() const;
	virtual void setStringDirect(const std::string& value);
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
	virtual bool needLoadSave() const;
	virtual bool needTransfer() const;
	virtual void additionalInfo(TclObject& result) const;

private:
	BaseSetting* getSetting();
	const BaseSetting* getSetting() const;

	Reactor& reactor;
};

} // namespace openmsx

#endif
