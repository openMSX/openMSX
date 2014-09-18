#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "RTSchedulable.hh"
#include "EventListener.hh"
#include "stl.hh"
#include "string_ref.hh"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>

namespace openmsx {

class Event;
class RTScheduler;
class GlobalCommandController;
class EventDistributor;
class XMLElement;
class BindCmd;
class UnbindCmd;
class ActivateCmd;
class DeactivateCmd;

class HotKey final : public RTSchedulable, public EventListener
{
public:
	struct HotKeyInfo {
		HotKeyInfo() {} // for map::operator[]
		HotKeyInfo(std::string command_, bool repeat_ = false)
			: command(std::move(command_)), repeat(repeat_) {}
		std::string command;
		bool repeat;
	};
	typedef std::shared_ptr<const Event> EventPtr;
	typedef std::map<EventPtr, HotKeyInfo, LessDeref> BindMap;
	typedef std::set<EventPtr,             LessDeref> KeySet;

	HotKey(RTScheduler& rtScheduler,
	       GlobalCommandController& commandController,
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
	void activateLayer  (std::string layer, bool blocking);
	void deactivateLayer(string_ref layer);

	int executeEvent(const EventPtr& event);
	void executeBinding(const EventPtr& event, const HotKeyInfo& info);
	void startRepeat  (const EventPtr& event);
	void stopRepeat();

	// EventListener
	virtual int signalEvent(const EventPtr& event);
	// RTSchedulable
	virtual void executeRT();

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
