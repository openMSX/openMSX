#ifndef IMGUI_MESSAGES_HH
#define IMGUI_MESSAGES_HH

#include "ImGuiCpp.hh"
#include "ImGuiPart.hh"

#include "CliListener.hh"

#include "circular_buffer.hh"

#include <array>
#include <string>

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
	explicit ImGuiMessages(ImGuiManager& manager_);
	~ImGuiMessages();

	[[nodiscard]] zstring_view iniName() const override { return "messages"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	im::WindowStatus logWindow;
	im::WindowStatus configureWindow;

private:
	void paintModal();
	void paintPopup();
	void paintProgress();
	void paintOSD();
	void paintLog();
	void paintConfigure();
	[[nodiscard]] bool paintButtons();

	void log(CliComm::LogLevel level, std::string_view message, float fraction);

private:
	enum PopupAction : int { NO_POPUP, POPUP, MODAL_POPUP };
	enum OpenLogAction : int { NO_OPEN_LOG, OPEN_LOG, OPEN_LOG_FOCUS };
	enum OsdAction : int { NO_OSD, SHOW_OSD };
	std::array<int /*PopupAction*/,   CliComm::NUM_LEVELS> popupAction;
	std::array<int /*OpenLogAction*/, CliComm::NUM_LEVELS> openLogAction;
	std::array<int /*OsdAction*/,     CliComm::NUM_LEVELS> osdAction;

	circular_buffer<Message> modalMessages;
	circular_buffer<Message> popupMessages;
	circular_buffer<Message> allMessages;
	std::string filterLog;
	bool doOpenModal = false;
	size_t doOpenPopup = 0;

	std::string progressMessage;
	float progressFraction = 0.0f;;
	float progressTime = 0.0f;
	bool doOpenProgress = false;

	struct Colors {
		uint32_t text;
		uint32_t background;
	};
	struct Step {
		float start;
		Colors colors;
	};
	using ColorSequence = std::array<Step, 4>;
	std::array<ColorSequence, 3> colorSequence = {
		ColorSequence{ // Info                  AABBGGRR
			Step{0.0f, Colors{0xff00ffff, 0x80ffffff}}, // start of flash
			Step{0.5f, Colors{0xffffffff, 0x80808080}}, // start of stable colors
			Step{5.0f, Colors{0xffffffff, 0x80808080}}, // end of stable colors
			Step{1.5f, Colors{0x00ffffff, 0x00808080}}, // end of fade-out
		},
		ColorSequence{ // warning
			Step{0.0f, Colors{0xff00ffff, 0x80ffffff}}, // start of flash
			Step{0.5f, Colors{0xffffffff, 0x800060A0}}, // start of stable colors
			Step{5.0f, Colors{0xffffffff, 0x800060A0}}, // end of stable colors
			Step{1.5f, Colors{0x00ffffff, 0x000060A0}}, // end of fade-out
		},
		ColorSequence{ // error
			Step{0.0f, Colors{0xff00ffff, 0x80ffffff}}, // start of flash
			Step{0.5f, Colors{0xffffffff, 0x800000C0}}, // start of stable colors
			Step{5.0f, Colors{0xffffffff, 0x800000C0}}, // end of stable colors
			Step{1.5f, Colors{0x00ffffff, 0x000000C0}}, // end of fade-out
		}
	};
	struct OsdMessage {
		// clang workaround:
		OsdMessage(const std::string& te, float ti, CliComm::LogLevel l)
			: text(te), time(ti), level(l) {}

		std::string text;
		float time;
		CliComm::LogLevel level;
	};
	std::vector<OsdMessage> osdMessages;

	struct Listener : CliListener {
		ImGuiMessages& messages;
		Listener(ImGuiMessages& m) : messages(m) {}

		void log(CliComm::LogLevel level, std::string_view message, float fraction) noexcept override {
			messages.log(level, message, fraction);
		}
		void update(CliComm::UpdateType /*type*/, std::string_view /*machine*/,
		            std::string_view /*name*/, std::string_view /*value*/) noexcept override {
			// nothing
		}
	};
	CliListener* listenerHandle;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showLog",       &ImGuiMessages::logWindow},
		PersistentElement{"showConfigure", &ImGuiMessages::configureWindow}
		// 'popupAction', 'openLogAction', 'osdAction' handled elsewhere
	};
};

} // namespace openmsx

#endif
