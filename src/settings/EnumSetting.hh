#ifndef ENUMSETTING_HH
#define ENUMSETTING_HH

#include "Setting.hh"
#include <iterator>
#include <utility>
#include <vector>

namespace openmsx {

class TclObject;

// non-templatized base class
class EnumSettingBase
{
protected:
	// cannot be string_ref because of the 'default_machine' setting
	typedef std::vector<std::pair<std::string, int>> BaseMap;
	EnumSettingBase(BaseMap&& m);

	int fromStringBase(string_ref str) const;
	string_ref toStringBase(int value) const;

	std::vector<string_ref> getPossibleValues() const;
	void additionalInfoBase(TclObject& result) const;
	void tabCompletionBase(std::vector<std::string>& tokens) const;

private:
	BaseMap baseMap;
};

template<typename T> class EnumSetting final : private EnumSettingBase, public Setting
{
public:
	typedef std::vector<std::pair<std::string, T>> Map;

	EnumSetting(CommandController& commandController, string_ref name,
	            string_ref description, T initialValue,
	            Map& map_, SaveSetting save = SAVE);

	string_ref getTypeString() const override;
	void additionalInfo(TclObject& result) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

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
	: EnumSettingBase(BaseMap(std::make_move_iterator(begin(map)),
	                          std::make_move_iterator(end(map))))
	, Setting(commandController, name, description,
	          toString(initialValue).str(), save)
{
	setChecker([this](TclObject& newValue) {
		fromStringBase(newValue.getString()); // may throw
	});
	init();
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
template<> inline bool EnumSetting<bool>::getEnum() const
{
	// _exactly_ the same functionality as above, but suppress VS warning
	return fromStringBase(getValue().getString()) != 0;
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
