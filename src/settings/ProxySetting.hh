// $Id$

#ifndef PROXYSETTINGNODE_HH
#define PROXYSETTINGNODE_HH

#include "Setting.hh"

namespace openmsx {

class GlobalCommandController;

class ProxySetting : public Setting
{
public:
	ProxySetting(GlobalCommandController& commandController,
	             const std::string& name);

	virtual std::string getTypeString() const;
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
};

} // namespace openmsx

#endif
