// $Id$

#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "EventListener.hh"
#include "Keys.hh"
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

class HotKey : private EventListener
{
public:
	HotKey(CommandController& commandController,
	       EventDistributor& eventDistributor);
	virtual ~HotKey();

	void loadBindings(const XMLElement& config);
	void saveBindings(XMLElement& config) const;

private:
	void initDefaultBindings();
	void bind  (Keys::KeyCode key, const std::string& command);
	void unbind(Keys::KeyCode key);
	void bindDefault  (Keys::KeyCode key, const std::string& command);
	void unbindDefault(Keys::KeyCode key);

	// EventListener
	virtual void signalEvent(const Event& event);

	friend class BindCmd;
	friend class UnbindCmd;
	friend class BindDefaultCmd;
	friend class UnbindDefaultCmd;
	const std::auto_ptr<BindCmd>          bindCmd;
	const std::auto_ptr<UnbindCmd>        unbindCmd;
	const std::auto_ptr<BindDefaultCmd>   bindDefaultCmd;
	const std::auto_ptr<UnbindDefaultCmd> unbindDefaultCmd;

	typedef std::map<Keys::KeyCode, std::string> BindMap;
	typedef std::set<Keys::KeyCode> KeySet;
	BindMap cmdMap;
	BindMap defaultMap;
	KeySet boundKeys;
	KeySet unboundKeys;
	CommandController& commandController;
	EventDistributor& eventDistributor;
	bool loading; // hack
};

} // namespace openmsx

#endif
