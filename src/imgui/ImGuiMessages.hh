#ifndef IMGUI_MESSAGES_HH
#define IMGUI_MESSAGES_HH

#include "ImGuiCpp.hh"
#include "ImGuiPart.hh"

#include "CliListener.hh"

#include "circular_buffer.hh"
#include "stl.hh"

#include <array>
#include <cstdint>
#include <string>

namespace openmsx {

class ImGuiManager;

class ImGuiMessages final : public ImGuiPart
{
public:
	struct Message {
		CliComm::LogLevel level;
		std::string text;
		uint64_t timestamp; // us
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
	array_with_enum_index<CliComm::LogLevel, int /*PopupAction*/  > popupAction;
	array_with_enum_index<CliComm::LogLevel, int /*OpenLogAction*/> openLogAction;
	array_with_enum_index<CliComm::LogLevel, int /*OsdAction*/    > osdAction;

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
	array_with_enum_index<CliComm::LogLevel, ColorSequence> colorSequence = {
		ColorSequence{ // Info                     AA'BB'GG'RR
			Step{.start = 0.0f, .colors = Colors{.text = 0xff'00'ff'ff, .background = 0x80'ff'ff'ff}}, // start of flash
			Step{.start = 0.5f, .colors = Colors{.text = 0xff'ff'ff'ff, .background = 0x80'80'80'80}}, // start of stable colors
			Step{.start = 5.0f, .colors = Colors{.text = 0xff'ff'ff'ff, .background = 0x80'80'80'80}}, // end of stable colors
			Step{.start = 1.5f, .colors = Colors{.text = 0x00'ff'ff'ff, .background = 0x00'80'80'80}}, // end of fade-out
		},
		ColorSequence{ // warning
			Step{.start = 0.0f, .colors = Colors{.text = 0xff'00'ff'ff, .background = 0x80'ff'ff'ff}}, // start of flash
			Step{.start = 0.5f, .colors = Colors{.text = 0xff'ff'ff'ff, .background = 0x80'00'60'A0}}, // start of stable colors
			Step{.start = 5.0f, .colors = Colors{.text = 0xff'ff'ff'ff, .background = 0x80'00'60'A0}}, // end of stable colors
			Step{.start = 1.5f, .colors = Colors{.text = 0x00'ff'ff'ff, .background = 0x00'00'60'A0}}, // end of fade-out
		},
		ColorSequence{ // error
			Step{.start = 0.0f, .colors = Colors{.text = 0xff'00'ff'ff, .background = 0x80'ff'ff'ff}}, // start of flash
			Step{.start = 0.5f, .colors = Colors{.text = 0xff'ff'ff'ff, .background = 0x80'00'00'C0}}, // start of stable colors
			Step{.start = 5.0f, .colors = Colors{.text = 0xff'ff'ff'ff, .background = 0x80'00'00'C0}}, // end of stable colors
			Step{.start = 1.5f, .colors = Colors{.text = 0x00'ff'ff'ff, .background = 0x00'00'00'C0}}, // end of fade-out
		},
		ColorSequence{ /*dummy*/ } // progress
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
		explicit Listener(ImGuiMessages& m) : messages(m) {}

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
