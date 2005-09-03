// $Id$

#ifndef ENUMSETTING_HH
#define ENUMSETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"
#include "InfoTopic.hh"
#include "InfoCommand.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include <map>
#include <set>
#include <cassert>

namespace openmsx {

class CommandController;


template <typename T> class EnumSettingPolicy : public SettingPolicy<T>
{
public:
	typedef std::map<std::string, T, StringOp::caseless> Map;

	void getPossibleValues(std::set<std::string>& result) const;

protected:
	EnumSettingPolicy(CommandController& commandController,
	                  const std::string& name, const Map& map_);
	virtual ~EnumSettingPolicy();

	std::string toString(T value) const;
	T fromString(const std::string& str) const;
	virtual void checkSetValue(T& value) const;
	void tabCompletion(std::vector<std::string>& tokens) const;

private:
	std::string name;
	Map enumMap;

	class EnumInfo : public InfoTopic {
	public:
		EnumInfo(CommandController& commandController,
		         EnumSettingPolicy& parent, const std::string& name);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		EnumSettingPolicy& parent;
	} enumInfo;
};

template <typename T> class EnumSetting : public SettingImpl<EnumSettingPolicy<T> >
{
public:
	EnumSetting(CommandController& CommandController, const std::string& name,
	            const std::string& description, T initialValue,
	            const typename EnumSettingPolicy<T>::Map& map_,
	            Setting::SaveSetting save = Setting::SAVE);
};


//-------------

template <typename T>
EnumSettingPolicy<T>::EnumSettingPolicy(
		CommandController& commandController,
		const std::string& name_, const Map& map_)
	: SettingPolicy<T>(commandController)
	, name(name_), enumMap(map_)
	, enumInfo(commandController, *this, name)
{
}

template <typename T>
EnumSettingPolicy<T>::~EnumSettingPolicy()
{
}

template<typename T>
void EnumSettingPolicy<T>::getPossibleValues(std::set<std::string>& result) const
{
	for (typename Map::const_iterator it = enumMap.begin();
	     it != enumMap.end(); ++it) {
		try {
			T val = it->second;
			checkSetValue(val);
			result.insert(it->first);
		} catch (MSXException& e) {
			// ignore
		}
	}
}

template<typename T>
std::string EnumSettingPolicy<T>::toString(T value) const
{
	for (typename Map::const_iterator it = enumMap.begin();
	     it != enumMap.end() ; ++it) {
		if (it->second == value) {
			return it->first;
		}
	}
	assert(false);
	return "";	// avoid warning
}

template<typename T>
T EnumSettingPolicy<T>::fromString(const std::string& str) const
{
	typename Map::const_iterator it = enumMap.find(str);
	if (it == enumMap.end()) {
		throw CommandException("not a valid value: " + str);
	}
	return it->second;
}

template<typename T>
void EnumSettingPolicy<T>::checkSetValue(T& /*value*/) const
{
}

template<typename T>
void EnumSettingPolicy<T>::tabCompletion(std::vector<std::string>& tokens) const
{
	std::set<std::string> stringSet;
	getPossibleValues(stringSet);
	this->getCommandController().completeString(tokens, stringSet, false); // case insensitive
}

template<typename T>
EnumSettingPolicy<T>::EnumInfo::EnumInfo(CommandController& commandController,
                                         EnumSettingPolicy& parent_,
                                         const std::string& name)
	: InfoTopic(commandController, name)
	, parent(parent_)
{
}

template<typename T>
void EnumSettingPolicy<T>::EnumInfo::execute(
	const std::vector<TclObject*>& /*tokens*/,
	TclObject& result) const
{
	std::set<std::string> values;
	parent.getPossibleValues(values);
	for (std::set<std::string>::const_iterator it = values.begin();
	     it != values.end(); ++it) {
		result.addListElement(*it);
	}
}

template<typename T>
std::string EnumSettingPolicy<T>::EnumInfo::help(
	const std::vector<std::string>& /*tokens*/) const
{
	return "Returns all possible values for this setting.";
}



template <typename T>
EnumSetting<T>::EnumSetting(
		CommandController& commandController, const std::string& name,
		const std::string& description, T initialValue,
		const typename EnumSettingPolicy<T>::Map& map_,
		Setting::SaveSetting save)
	: SettingImpl<EnumSettingPolicy<T> >(
		commandController, name, description, initialValue, save,
		name, map_)
{
}

} // namespace openmsx

#endif
