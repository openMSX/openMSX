#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "RTSchedulable.hh"
#include "EventListener.hh"
#include "Command.hh"
#include "stl.hh"
#include "string_view.hh"
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

class HotKey final : public RTSchedulable, public EventListener
{
public:
	struct HotKeyInfo {
		HotKeyInfo() {} // for map::operator[]
		explicit HotKeyInfo(std::string command_, bool repeat_ = false)
			: command(std::move(command_)), repeat(repeat_) {}
		std::string command;
		bool repeat;
	};
	using EventPtr = std::shared_ptr<const Event>;
	using BindMap  = std::map<EventPtr, HotKeyInfo, LessDeref>;
	using KeySet   = std::set<EventPtr,             LessDeref>;

	HotKey(RTScheduler& rtScheduler,
	       GlobalCommandController& commandController,
	       EventDistributor& eventDistributor);
	~HotKey();

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
	void deactivateLayer(string_view layer);

	int executeEvent(const EventPtr& event);
	void executeBinding(const EventPtr& event, const HotKeyInfo& info);
	void startRepeat  (const EventPtr& event);
	void stopRepeat();

	// EventListener
	int signalEvent(const EventPtr& event) override;
	// RTSchedulable
	void executeRT() override;

	class BindCmd final : public Command {
	public:
		BindCmd(CommandController& commandController, HotKey& hotKey,
			bool defaultCmd);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	private:
		std::string formatBinding(const HotKey::BindMap::value_type& p);

		HotKey& hotKey;
		const bool defaultCmd;
	};
	BindCmd bindCmd;
	BindCmd bindDefaultCmd;

	class UnbindCmd final : public Command {
	public:
		UnbindCmd(CommandController& commandController, HotKey& hotKey,
			  bool defaultCmd);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	private:
		HotKey& hotKey;
		const bool defaultCmd;
	};
	UnbindCmd unbindCmd;
	UnbindCmd unbindDefaultCmd;

	struct ActivateCmd final : Command {
		explicit ActivateCmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} activateCmd;

	struct DeactivateCmd final : Command {
		explicit DeactivateCmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} deactivateCmd;

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
