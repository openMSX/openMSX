#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "RTSchedulable.hh"
#include "EventListener.hh"
#include "Command.hh"
#include <map>
#include <string_view>
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
	using EventPtr = std::shared_ptr<const Event>;
	struct HotKeyInfo {
		HotKeyInfo(const EventPtr& event_, std::string command_,
		           bool repeat_ = false, bool passEvent_ = false)
			: event(event_), command(std::move(command_))
			, repeat(repeat_)
			, passEvent(passEvent_) {}
		EventPtr event;
		std::string command;
		bool repeat;
		bool passEvent; // whether to pass event with args back to command
	};
	using BindMap = std::vector<HotKeyInfo>; // unsorted
	using KeySet  = std::vector<EventPtr>;   // unsorted

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
	void bind         (HotKeyInfo&& info);
	void unbind       (const EventPtr& event);
	void bindDefault  (HotKeyInfo&& info);
	void unbindDefault(const EventPtr& event);
	void bindLayer    (HotKeyInfo&& info, const std::string& layer);
	void unbindLayer  (const EventPtr& event, const std::string& layer);
	void unbindFullLayer(const std::string& layer);
	void activateLayer  (std::string layer, bool blocking);
	void deactivateLayer(std::string_view layer);

	int executeEvent(const EventPtr& event);
	void executeBinding(const EventPtr& event, const HotKeyInfo& info);
	void startRepeat  (const EventPtr& event);
	void stopRepeat();

	// EventListener
	int signalEvent(const EventPtr& event) noexcept override;
	// RTSchedulable
	void executeRT() override;

private:
	class BindCmd final : public Command {
	public:
		BindCmd(CommandController& commandController, HotKey& hotKey,
			bool defaultCmd);
		void execute(span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(const std::vector<std::string>& tokens) const override;
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
		void execute(span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(const std::vector<std::string>& tokens) const override;
	private:
		HotKey& hotKey;
		const bool defaultCmd;
	};
	UnbindCmd unbindCmd;
	UnbindCmd unbindDefaultCmd;

	struct ActivateCmd final : Command {
		explicit ActivateCmd(CommandController& commandController);
		void execute(span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(const std::vector<std::string>& tokens) const override;
	} activateCmd;

	struct DeactivateCmd final : Command {
		explicit DeactivateCmd(CommandController& commandController);
		void execute(span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(const std::vector<std::string>& tokens) const override;
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
