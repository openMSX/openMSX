#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "RTSchedulable.hh"
#include "EventListener.hh"
#include "Command.hh"
#include "Event.hh"
#include "TclObject.hh"
#include <map>
#include <string_view>
#include <vector>
#include <string>

namespace openmsx {

class RTScheduler;
class GlobalCommandController;
class EventDistributor;

class HotKey final : public RTSchedulable, public EventListener
{
public:
	struct HotKeyInfo {
		HotKeyInfo(Event event_, std::string command_,
		           bool repeat_ = false, bool passEvent_ = false)
			: event(std::move(event_)), command(std::move(command_))
			, repeat(repeat_)
			, passEvent(passEvent_) {}
		Event event;
		std::string command;
		bool repeat;
		bool passEvent; // whether to pass event with args back to command
	};
	using BindMap = std::vector<HotKeyInfo>; // unsorted
	using KeySet  = std::vector<Event>;   // unsorted

	HotKey(RTScheduler& rtScheduler,
	       GlobalCommandController& commandController,
	       EventDistributor& eventDistributor);
	~HotKey();

	void loadInit();
	void loadBind(std::string_view key, std::string_view cmd, bool repeat, bool event);
	void loadUnbind(std::string_view key);

	template<typename XmlStream>
	void saveBindings(XmlStream& xml) const
	{
		xml.begin("bindings");
		// add explicit bind's
		for (const auto& k : boundKeys) {
			xml.begin("bind");
			xml.attribute("key", toString(k));
			const auto& info = *find_unguarded(cmdMap, k, &HotKeyInfo::event);
			if (info.repeat) {
				xml.attribute("repeat", "true");
			}
			if (info.passEvent) {
				xml.attribute("event", "true");
			}
			xml.data(info.command);
			xml.end("bind");
		}
		// add explicit unbinds
		for (const auto& k : unboundKeys) {
			xml.begin("unbind");
			xml.attribute("key", toString(k));
			xml.end("unbind");
		}
		xml.end("bindings");
	}

private:
	struct LayerInfo {
		std::string layer;
		bool blocking;
	};

	void initDefaultBindings();
	void bind         (HotKeyInfo&& info);
	void unbind       (const Event& event);
	void bindDefault  (HotKeyInfo&& info);
	void unbindDefault(const Event& event);
	void bindLayer    (HotKeyInfo&& info, const std::string& layer);
	void unbindLayer  (const Event& event, const std::string& layer);
	void unbindFullLayer(const std::string& layer);
	void activateLayer  (std::string layer, bool blocking);
	void deactivateLayer(std::string_view layer);

	int executeEvent(const Event& event);
	void executeBinding(const Event& event, const HotKeyInfo& info);
	void startRepeat  (const Event& event);
	void stopRepeat();

	// EventListener
	int signalEvent(const Event& event) override;
	// RTSchedulable
	void executeRT() override;

private:
	class BindCmd final : public Command {
	public:
		BindCmd(CommandController& commandController, HotKey& hotKey,
			bool defaultCmd);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	private:
		HotKey& hotKey;
		const bool defaultCmd;
	};
	BindCmd bindCmd;
	BindCmd bindDefaultCmd;

	class UnbindCmd final : public Command {
	public:
		UnbindCmd(CommandController& commandController, HotKey& hotKey,
			  bool defaultCmd);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	private:
		HotKey& hotKey;
		const bool defaultCmd;
	};
	UnbindCmd unbindCmd;
	UnbindCmd unbindDefaultCmd;

	struct ActivateCmd final : Command {
		explicit ActivateCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} activateCmd;

	struct DeactivateCmd final : Command {
		explicit DeactivateCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} deactivateCmd;

	BindMap cmdMap;
	BindMap defaultMap;
	std::map<std::string, BindMap> layerMap;
	std::vector<LayerInfo> activeLayers;
	KeySet boundKeys;
	KeySet unboundKeys;
	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	Event lastEvent;
};

} // namespace openmsx

#endif
