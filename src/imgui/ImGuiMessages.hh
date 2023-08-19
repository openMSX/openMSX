#ifndef IMGUI_MESSAGES_HH
#define IMGUI_MESSAGES_HH

#include "ImGuiPart.hh"

#include "CliListener.hh"

#include "circular_buffer.hh"

#include <array>
#include <string>
#include <vector>

namespace openmsx {

class ImGuiManager;

class ImGuiMessages final : public ImGuiPart
{
public:
	struct Message {
		CliComm::LogLevel level;
		std::string text;
	};
public:
	ImGuiMessages(ImGuiManager& manager_);
	~ImGuiMessages();

	[[nodiscard]] zstring_view iniName() const override { return "messages"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool showLog       = false;
	bool showConfigure = false;

private:
	void paintModal();
	void paintPopup();
	void paintLog();
	void paintConfigure();
	[[nodiscard]] bool paintButtons();

	void log(CliComm::LogLevel level, std::string_view message);

private:
	ImGuiManager& manager;
	enum PopupAction : int { NO_POPUP, POPUP, MODAL_POPUP };
	enum OpenLogAction : int { NO_OPEN_LOG, OPEN_LOG, OPEN_LOG_FOCUS };
	std::array<PopupAction, CliComm::NUM_LEVELS> popupAction;
	std::array<OpenLogAction, CliComm::NUM_LEVELS> openLogAction;

	circular_buffer<Message> modalMessages;
	circular_buffer<Message> popupMessages;
	circular_buffer<Message> allMessages;
	std::string filterLog;
	bool doOpenModal = false;
	size_t doOpenPopup = 0;
	bool focusLog = false;

	struct Listener : CliListener {
		ImGuiMessages& messages;
		Listener(ImGuiMessages& m) : messages(m) {}

		void log(CliComm::LogLevel level, std::string_view message) noexcept override {
			messages.log(level, message);
		}
		void update(CliComm::UpdateType /*type*/, std::string_view /*machine*/,
		            std::string_view /*name*/, std::string_view /*value*/) noexcept override {
			// nothing
		}
	};
	CliListener* listenerHandle;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showLog",       &ImGuiMessages::showLog},
		PersistentElement{"showConfigure", &ImGuiMessages::showConfigure}
		// 'popupAction', 'openLogAction' handled elsewhere
	};
};

} // namespace openmsx

#endif
