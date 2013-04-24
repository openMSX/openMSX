#ifndef PROXYSETTINGNODE_HH
#define PROXYSETTINGNODE_HH

#include "Setting.hh"

namespace openmsx {

class CommandController;
class Reactor;

class ProxySetting : public Setting
{
public:
	ProxySetting(CommandController& commandController,
	             Reactor& reactor, string_ref name);

	virtual string_ref getTypeString() const;
	virtual std::string getDescription() const;
	virtual std::string getValueString() const;
	virtual std::string getDefaultValueString() const;
	virtual std::string getRestoreValueString() const;
	virtual void setValueStringDirect(const std::string& valueString);
	virtual void restoreDefault();
	virtual bool hasDefaultValue() const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
	virtual bool needLoadSave() const;
	virtual void setDontSaveValue(const std::string& dontSaveValue);
	virtual void additionalInfo(TclObject& result) const;

private:
	Setting* getSetting();
	const Setting* getSetting() const;

	Reactor& reactor;
};

} // namespace openmsx

#endif
