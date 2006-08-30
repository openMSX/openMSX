// $Id$

#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "EventListener.hh"
#include "shared_ptr.hh"
#include <map>
#include <set>
#include <string>
#include <memory>

namespace openmsx {

class CommandController;
class EventDistributor;
class XMLElement;
class BindCmd;
class UnbindCmd;
class BindDefaultCmd;
class UnbindDefaultCmd;
class InputEvent;

template<typename T> struct deref_less
{
	bool operator()(T t1, T t2) const { return *t1 < *t2; }
};

class HotKey : private EventListener
{
public:
	typedef shared_ptr<const Event> EventPtr;
	HotKey(CommandController& commandController,
	       EventDistributor& eventDistributor);
	virtual ~HotKey();

	void loadBindings(const XMLElement& config);
	void saveBindings(XMLElement& config) const;

private:
	void initDefaultBindings();
	void bind  (EventPtr event, const std::string& command);
	void unbind(EventPtr event);
	void bindDefault  (EventPtr event, const std::string& command);
	void unbindDefault(EventPtr event);

	// EventListener
	virtual void signalEvent(EventPtr event);

	friend class BindCmd;
	friend class UnbindCmd;
	friend class BindDefaultCmd;
	friend class UnbindDefaultCmd;
	const std::auto_ptr<BindCmd>          bindCmd;
	const std::auto_ptr<UnbindCmd>        unbindCmd;
	const std::auto_ptr<BindDefaultCmd>   bindDefaultCmd;
	const std::auto_ptr<UnbindDefaultCmd> unbindDefaultCmd;

	typedef std::map<EventPtr, std::string, deref_less<EventPtr> > BindMap;
	typedef std::set<EventPtr,              deref_less<EventPtr> > KeySet;
	BindMap cmdMap;
	BindMap defaultMap;
	KeySet boundKeys;
	KeySet unboundKeys;
	CommandController& commandController;
	EventDistributor& eventDistributor;
};

} // namespace openmsx

#endif
