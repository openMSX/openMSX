#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "EventListener.hh"
#include "noncopyable.hh"
#include "stl.hh"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>

namespace openmsx {

class Event;
class GlobalCommandController;
class EventDistributor;
class XMLElement;
class BindCmd;
class UnbindCmd;
class ActivateCmd;
class DeactivateCmd;
class AlarmEvent;

class HotKey : public EventListener, private noncopyable
{
public:
	struct HotKeyInfo {
		HotKeyInfo() {} // for map::operator[]
		HotKeyInfo(const std::string& command_, bool repeat_ = false)
			: command(command_), repeat(repeat_) {}
		std::string command;
		bool repeat;
	};
	typedef std::shared_ptr<const Event> EventPtr;
	typedef std::map<EventPtr, HotKeyInfo, LessDeref> BindMap;
	typedef std::set<EventPtr,             LessDeref> KeySet;

	HotKey(GlobalCommandController& commandController,
	       EventDistributor& eventDistributor);
	virtual ~HotKey();

	void loadBindings(const XMLElement& config);
	void saveBindings(XMLElement& config) const;

private:
	struct LayerInfo {
		std::string layer;
		bool blocking;
	};

	void initDefaultBindings();
	void bind         (const EventPtr& event, const HotKeyInfo& info);
	void unbind       (const EventPtr& event);
	void bindDefault  (const EventPtr& event, const HotKeyInfo& info);
	void unbindDefault(const EventPtr& event);
	void bindLayer    (const EventPtr& event, const HotKeyInfo& info,
	                   const std::string& layer);
	void unbindLayer  (const EventPtr& event, const std::string& layer);
	void unbindFullLayer(const std::string& layer);
	void activateLayer  (const std::string& layer, bool blocking);
	void deactivateLayer(const std::string& layer);

	void executeBinding(const EventPtr& event, const HotKeyInfo& info);
	void startRepeat  (const EventPtr& event);
	void stopRepeat();

	// EventListener
	virtual int signalEvent(const EventPtr& event);

	friend class BindCmd;
	friend class UnbindCmd;
	friend class ActivateCmd;
	friend class DeactivateCmd;
	const std::unique_ptr<BindCmd>       bindCmd;
	const std::unique_ptr<UnbindCmd>     unbindCmd;
	const std::unique_ptr<BindCmd>       bindDefaultCmd;
	const std::unique_ptr<UnbindCmd>     unbindDefaultCmd;
	const std::unique_ptr<ActivateCmd>   activateCmd;
	const std::unique_ptr<DeactivateCmd> deactivateCmd;
	const std::unique_ptr<AlarmEvent>    repeatAlarm;

	BindMap cmdMap;
	BindMap defaultMap;
	std::map<std::string, BindMap> layerMap;
	std::vector<LayerInfo> activeLayers;
	KeySet boundKeys;
	KeySet unboundKeys;
	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	EventPtr lastEvent;
};

} // namespace openmsx

#endif
