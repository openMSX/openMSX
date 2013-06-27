#ifndef ENUMSETTING_HH
#define ENUMSETTING_HH

#include "Setting.hh"
#include <map>

namespace openmsx {

class TclObject;

// non-templatized base class
class EnumSettingBase
{
protected:
	typedef std::map<std::string, int> BaseMap;
	EnumSettingBase(BaseMap&& m);

	int fromStringBase(string_ref str) const;
	string_ref toStringBase(int value) const;

	std::vector<string_ref> getPossibleValues() const;
	void additionalInfoBase(TclObject& result) const;
	void tabCompletionBase(std::vector<std::string>& tokens) const;

private:
	BaseMap baseMap;
};

template<typename T> class EnumSetting : private EnumSettingBase, public Setting
{
public:
	typedef std::map<std::string, T> Map;

	EnumSetting(CommandController& commandController, string_ref name,
	            string_ref description, T initialValue,
	            Map& map_, SaveSetting save = SAVE);

	virtual string_ref getTypeString() const;
	virtual void additionalInfo(TclObject& result) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	T getEnum() const;
	void setEnum(T value);

private:
	string_ref toString(T value) const;
};


//-------------


template <typename T>
EnumSetting<T>::EnumSetting(
		CommandController& commandController, string_ref name,
		string_ref description, T initialValue,
		Map& map, SaveSetting save)
	: EnumSettingBase(BaseMap(map.begin(), map.end()))
	, Setting(commandController, name, description,
	          toString(initialValue).str(), save)
{
	setChecker([this](TclObject& newValue) {
		fromStringBase(newValue.getString()); // may throw
	});
}

template<typename T>
string_ref EnumSetting<T>::getTypeString() const
{
	return "enumeration";
}

template<typename T>
void EnumSetting<T>::additionalInfo(TclObject& result) const
{
	additionalInfoBase(result);
}

template<typename T>
void EnumSetting<T>::tabCompletion(std::vector<std::string>& tokens) const
{
	tabCompletionBase(tokens);
}

template<typename T>
T EnumSetting<T>::getEnum() const
{
	return static_cast<T>(fromStringBase(getValue().getString()));
}

template<typename T>
void EnumSetting<T>::setEnum(T value)
{
	setString(toString(value).str());
}

template<typename T>
string_ref EnumSetting<T>::toString(T value) const
{
	return toStringBase(static_cast<int>(value));
}

} // namespace openmsx

#endif
