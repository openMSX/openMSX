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
	// cannot be string_view because of the 'default_machine' setting
	using BaseMap = std::vector<std::pair<std::string, int>>;
	explicit EnumSettingBase(BaseMap&& m);

	int fromStringBase(std::string_view str) const;
	std::string_view toStringBase(int value) const;

	std::vector<std::string_view> getPossibleValues() const;
	void additionalInfoBase(TclObject& result) const;
	void tabCompletionBase(std::vector<std::string>& tokens) const;

private:
	BaseMap baseMap;
};

template<typename T> class EnumSetting final : private EnumSettingBase, public Setting
{
public:
	using Map = std::vector<std::pair<std::string, T>>;

	EnumSetting(CommandController& commandController, std::string_view name,
	            std::string_view description, T initialValue,
	            Map&& map_, SaveSetting save = SAVE);

	std::string_view getTypeString() const override;
	void additionalInfo(TclObject& result) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	T getEnum() const noexcept;
	void setEnum(T e);
	std::string_view getString() const;

private:
	std::string_view toString(T e) const;
};


//-------------


template <typename T>
EnumSetting<T>::EnumSetting(
		CommandController& commandController_, std::string_view name,
		std::string_view description_, T initialValue,
		Map&& map, SaveSetting save_)
	: EnumSettingBase(BaseMap(std::move_iterator(begin(map)),
	                          std::move_iterator(end(map))))
	, Setting(commandController_, name, description_,
	          TclObject(toString(initialValue)), save_)
{
	setChecker([this](TclObject& newValue) {
		fromStringBase(newValue.getString()); // may throw
	});
	init();
}

template<typename T>
std::string_view EnumSetting<T>::getTypeString() const
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
T EnumSetting<T>::getEnum() const noexcept
{
	return static_cast<T>(fromStringBase(getValue().getString()));
}
template<> inline bool EnumSetting<bool>::getEnum() const noexcept
{
	// _exactly_ the same functionality as above, but suppress VS warning
	return fromStringBase(getValue().getString()) != 0;
}

template<typename T>
void EnumSetting<T>::setEnum(T e)
{
	setValue(TclObject(toString(e)));
}

template<typename T>
std::string_view EnumSetting<T>::getString() const
{
	return getValue().getString();
}

template<typename T>
std::string_view EnumSetting<T>::toString(T e) const
{
	return toStringBase(static_cast<int>(e));
}

} // namespace openmsx

#endif
