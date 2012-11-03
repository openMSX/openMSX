// $Id$

#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "Event.hh"
#include "EventListener.hh"
#include "noncopyable.hh"
#include "shared_ptr.hh"
#include <map>
#include <set>
#include <string>
#include <memory>

namespace openmsx {

class GlobalCommandController;
class EventDistributor;
class XMLElement;
class BindCmd;
class UnbindCmd;
class AlarmEvent;

template<typename T> struct deref_less
{
	bool operator()(T t1, T t2) const { return *t1 < *t2; }
};

class HotKey : private EventListener, private noncopyable
{
public:
	typedef shared_ptr<const Event> EventPtr;
	HotKey(GlobalCommandController& commandController,
	       EventDistributor& eventDistributor);
	virtual ~HotKey();

	void loadBindings(const XMLElement& config);
	void saveBindings(XMLElement& config) const;

private:
	struct HotKeyInfo {
		HotKeyInfo() {} // for map::operator[]
		HotKeyInfo(const std::string& command_, bool repeat_ = false)
			: command(command_), repeat(repeat_) {}
		std::string command;
		bool repeat;
	};
	typedef std::map<EventPtr, HotKeyInfo, deref_less<EventPtr> > BindMap;
	typedef std::set<EventPtr,             deref_less<EventPtr> > KeySet;

	void initDefaultBindings();
	void bind         (const EventPtr& event, const HotKeyInfo& info);
	void unbind       (const EventPtr& event);
	void bindDefault  (const EventPtr& event, const HotKeyInfo& info);
	void unbindDefault(const EventPtr& event);
	void startRepeat  (const EventPtr& event);
	void stopRepeat();

	// EventListener
	virtual int signalEvent(const EventPtr& event);

	friend class BindCmd;
	friend class UnbindCmd;
	const std::unique_ptr<BindCmd>   bindCmd;
	const std::unique_ptr<UnbindCmd> unbindCmd;
	const std::unique_ptr<BindCmd>   bindDefaultCmd;
	const std::unique_ptr<UnbindCmd> unbindDefaultCmd;
	const std::unique_ptr<AlarmEvent> repeatAlarm;

	BindMap cmdMap;
	BindMap defaultMap;
	KeySet boundKeys;
	KeySet unboundKeys;
	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	EventPtr lastEvent;
};

} // namespace openmsx

#endif
