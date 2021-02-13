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
public:
	struct MapEntry {
		template<typename Enum>
		MapEntry(std::string_view name_, Enum value_)
			: name(name_), value(static_cast<int>(value_)) {}

		std::string name; // cannot be string_view because of the 'default_machine' setting
		int value;
	};
	using Map = std::vector<MapEntry>;
protected:

	explicit EnumSettingBase(Map&& m);

	[[nodiscard]] int fromStringBase(std::string_view str) const;
	[[nodiscard]] std::string_view toStringBase(int value) const;

	[[nodiscard]] std::vector<std::string_view> getPossibleValues() const;
	void additionalInfoBase(TclObject& result) const;
	void tabCompletionBase(std::vector<std::string>& tokens) const;

private:
	Map baseMap;
};

template<typename T> class EnumSetting final : private EnumSettingBase, public Setting
{
public:
	using Map = EnumSettingBase::Map;

	EnumSetting(CommandController& commandController, std::string_view name,
	            static_string_view description, T initialValue,
	            Map&& map_, SaveSetting save = SAVE);

	[[nodiscard]] std::string_view getTypeString() const override;
	void additionalInfo(TclObject& result) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] T getEnum() const noexcept;
	void setEnum(T e);
	[[nodiscard]] std::string_view getString() const;

private:
	std::string_view toString(T e) const;
};


//-------------


template<typename T>
EnumSetting<T>::EnumSetting(
		CommandController& commandController_, std::string_view name,
		static_string_view description_, T initialValue,
		Map&& map, SaveSetting save_)
	: EnumSettingBase(std::move(map))
	, Setting(commandController_, name, description_,
	          TclObject(EnumSettingBase::toStringBase(static_cast<int>(initialValue))), save_)
{
	setChecker([this](TclObject& newValue) {
		(void)fromStringBase(newValue.getString()); // may throw
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
