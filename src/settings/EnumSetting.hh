// $Id$

#ifndef ENUMSETTING_HH
#define ENUMSETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"
#include <map>
#include <set>

namespace openmsx {

class TclObject;

// non-templatized base class for EnumSettingPolicy<T>
class EnumSettingPolicyBase
{
public:
	void getPossibleValues(std::set<std::string>& result) const;

protected:
	virtual ~EnumSettingPolicyBase() {}
	void additionalInfoBase(TclObject& result) const;
	void tabCompletionBase(std::vector<std::string>& tokens) const;
	int fromStringBase(const std::string& str) const;
	std::string toStringBase(int value) const;
	virtual void checkSetValueBase(int& value) const = 0;

	typedef std::map<std::string, int> BaseMap;
	BaseMap baseMap;
};

template <typename T> class EnumSettingPolicy
	: public EnumSettingPolicyBase, public SettingPolicy<T>
{
public:
	typedef std::map<std::string, T> Map;

protected:
	EnumSettingPolicy(const Map& map_);
	virtual ~EnumSettingPolicy();

	std::string toString(T value) const;
	T fromString(const std::string& str) const;
	virtual void checkSetValue(T& value) const;
	void tabCompletion(std::vector<std::string>& tokens) const;
	std::string getTypeString() const;
	void additionalInfo(TclObject& result) const;

private:
	virtual void checkSetValueBase(int& value) const;
};


template <typename T> class EnumSetting : public SettingImpl<EnumSettingPolicy<T> >
{
public:
	EnumSetting(CommandController& commandController, const std::string& name,
	            const std::string& description, T initialValue,
	            const typename EnumSettingPolicy<T>::Map& map_,
	            Setting::SaveSetting save = Setting::SAVE);
	EnumSetting(CommandController& commandController, const char* name,
	            const char* description, T initialValue,
	            const typename EnumSettingPolicy<T>::Map& map_,
	            Setting::SaveSetting save = Setting::SAVE);
};


//-------------

template <typename T>
EnumSettingPolicy<T>::EnumSettingPolicy(const Map& map)
	: SettingPolicy<T>()
{
	baseMap.insert(map.begin(), map.end());
}

template <typename T>
EnumSettingPolicy<T>::~EnumSettingPolicy()
{
}

template<typename T>
std::string EnumSettingPolicy<T>::toString(T value) const
{
	return toStringBase(static_cast<int>(value));
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800)
// C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
#endif
template<typename T>
T EnumSettingPolicy<T>::fromString(const std::string& str) const
{
	return static_cast<T>(fromStringBase(str));
}

template<typename T>
void EnumSettingPolicy<T>::checkSetValueBase(int& value) const
{
	T t = static_cast<T>(value);
	checkSetValue(t);
	value = static_cast<int>(t);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template<typename T>
void EnumSettingPolicy<T>::checkSetValue(T& /*value*/) const
{
}

template<typename T>
void EnumSettingPolicy<T>::tabCompletion(std::vector<std::string>& tokens) const
{
	tabCompletionBase(tokens);
}

template<typename T>
std::string EnumSettingPolicy<T>::getTypeString() const
{
	return "enumeration";
}

template<typename T>
void EnumSettingPolicy<T>::additionalInfo(TclObject& result) const
{
	additionalInfoBase(result);
}


template <typename T>
EnumSetting<T>::EnumSetting(
		CommandController& commandController, const std::string& name,
		const std::string& description, T initialValue,
		const typename EnumSettingPolicy<T>::Map& map_,
		Setting::SaveSetting save)
	: SettingImpl<EnumSettingPolicy<T> >(
		commandController, name, description, initialValue, save, map_)
{
}

template <typename T>
EnumSetting<T>::EnumSetting(
		CommandController& commandController, const char* name,
		const char* description, T initialValue,
		const typename EnumSettingPolicy<T>::Map& map_,
		Setting::SaveSetting save)
	: SettingImpl<EnumSettingPolicy<T> >(
		commandController, name, description, initialValue, save, map_)
{
}

} // namespace openmsx

#endif
