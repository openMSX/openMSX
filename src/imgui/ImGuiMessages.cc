#include "ImGuiMessages.hh"

#include "CustomFont.h"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "GlobalCliComm.hh"
#include "PixelOperations.hh"
#include "Reactor.hh"

#include "stl.hh"

#include <imgui_stdlib.h>
#include <imgui.h>

#include <cassert>
#include <concepts>

namespace openmsx {

using namespace std::literals;

ImGuiMessages::ImGuiMessages(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, modalMessages(10)
	, popupMessages(10)
	, allMessages(10)
{
	popupAction[CliComm::INFO] = NO_POPUP;
	popupAction[CliComm::WARNING] = POPUP;
	popupAction[CliComm::LOGLEVEL_ERROR] = MODAL_POPUP;
	popupAction[CliComm::PROGRESS] = NO_POPUP;
	openLogAction[CliComm::INFO] = NO_OPEN_LOG;
	openLogAction[CliComm::WARNING] = NO_OPEN_LOG;
	openLogAction[CliComm::LOGLEVEL_ERROR] = NO_OPEN_LOG;
	openLogAction[CliComm::PROGRESS] = NO_OPEN_LOG;
	osdAction[CliComm::INFO] = SHOW_OSD;
	osdAction[CliComm::WARNING] = NO_OSD;
	osdAction[CliComm::LOGLEVEL_ERROR] = NO_OSD;
	osdAction[CliComm::PROGRESS] = NO_OSD;

	auto& reactor = manager.getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	listenerHandle = cliComm.addListener(std::make_unique<Listener>(*this));
}

ImGuiMessages::~ImGuiMessages()
{
	auto& reactor = manager.getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	cliComm.removeListener(*listenerHandle);
}

void ImGuiMessages::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	buf.appendf("popupActions=[%d %d %d]\n",
		popupAction[CliComm::LOGLEVEL_ERROR],
		popupAction[CliComm::WARNING],
		popupAction[CliComm::INFO]);
	buf.appendf("openLogActions=[%d %d %d]\n",
		openLogAction[CliComm::LOGLEVEL_ERROR],
		openLogAction[CliComm::WARNING],
		openLogAction[CliComm::INFO]);
	buf.appendf("osdActions=[%d %d %d]\n",
		osdAction[CliComm::LOGLEVEL_ERROR],
		osdAction[CliComm::WARNING],
		osdAction[CliComm::INFO]);
	buf.appendf("fadeOutDuration=[%f %f %f]\n",
		double(colorSequence[CliComm::LOGLEVEL_ERROR][2].start), // note: cast to double only to silence warnings
		double(colorSequence[CliComm::WARNING][2].start),
		double(colorSequence[CliComm::INFO][2].start));
}

void ImGuiMessages::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "popupActions"sv) {
		std::array<int, 3> a = {};
		if (sscanf(value.c_str(), "[%d %d %d]", &a[0], &a[1], &a[2]) == 3) {
			popupAction[CliComm::LOGLEVEL_ERROR] = PopupAction(a[0]);
			popupAction[CliComm::WARNING]        = PopupAction(a[1]);
			popupAction[CliComm::INFO]           = PopupAction(a[2]);
		}
	} else if (name == "openLogActions"sv) {
		std::array<int, 3> a = {};
		if (sscanf(value.c_str(), "[%d %d %d]", &a[0], &a[1], &a[2]) == 3) {
			openLogAction[CliComm::LOGLEVEL_ERROR] = OpenLogAction(a[0]);
			openLogAction[CliComm::WARNING]        = OpenLogAction(a[1]);
			openLogAction[CliComm::INFO]           = OpenLogAction(a[2]);
		}
	} else if (name == "osdActions"sv) {
		std::array<int, 3> a = {};
		if (sscanf(value.c_str(), "[%d %d %d]", &a[0], &a[1], &a[2]) == 3) {
			osdAction[CliComm::LOGLEVEL_ERROR] = OsdAction(a[0]);
			osdAction[CliComm::WARNING]        = OsdAction(a[1]);
			osdAction[CliComm::INFO]           = OsdAction(a[2]);
		}
	} else if (name == "fadeOutDuration"sv) {
		std::array<float, 3> a = {};
		if (sscanf(value.c_str(), "[%f %f %f]", &a[0], &a[1], &a[2]) == 3) {
			colorSequence[CliComm::LOGLEVEL_ERROR][2].start = a[0];
			colorSequence[CliComm::WARNING][2].start        = a[1];
			colorSequence[CliComm::INFO][2].start           = a[2];
		}
	}
}

void ImGuiMessages::paint(MSXMotherBoard* /*motherBoard*/)
{
	paintModal();
	paintPopup();
	paintProgress();
	paintOSD();
	if (logWindow.open) paintLog();
	if (configureWindow.open) paintConfigure();
}

template<std::predicate<std::string_view, std::string_view> Filter = always_true>
static void printMessages(const circular_buffer<ImGuiMessages::Message>& messages, Filter filter = {})
{
	im::TextWrapPos(ImGui::GetFontSize() * 50.0f, [&]{
		for (const auto& message : messages) {
			auto [color, prefix_] = [&]() -> std::pair<ImU32, std::string_view> {
				switch (message.level) {
					case CliComm::LOGLEVEL_ERROR: return {getColor(imColor::ERROR),   "Error:"};
					case CliComm::WARNING:        return {getColor(imColor::WARNING), "Warning:"};
					case CliComm::INFO:           return {getColor(imColor::TEXT),    "Info:"};
					default: assert(false);       return {getColor(imColor::TEXT),    "Unknown:"};
				}
			}();
			auto prefix = prefix_; // clang workaround
			if (std::invoke(filter, prefix, message.text)) {
				im::StyleColor(ImGuiCol_Text, color, [&]{
					ImGui::TextUnformatted(prefix);
					ImGui::SameLine();
					ImGui::TextUnformatted(message.text);
				});
			}
		}
	});
}

bool ImGuiMessages::paintButtons()
{
	ImGui::SetCursorPosX(40.0f);
	bool close = ImGui::Button("Ok");
	ImGui::SameLine(0.0f, 30.0f);
	if (ImGui::SmallButton("Configure...")) {
		close = true;
		configureWindow.raise();
	}
	return close;
}

void ImGuiMessages::paintModal()
{
	if (doOpenModal) {
		doOpenModal = false;
		ImGui::OpenPopup("Message");
	}

	bool open = true;
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
	im::PopupModal("Message", &open, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		printMessages(modalMessages);
		bool close = paintButtons();
		if (!open || close) {
			modalMessages.clear();
			ImGui::CloseCurrentPopup();
		}
	});
}

void ImGuiMessages::paintPopup()
{
	// TODO automatically fade-out and close
	if (doOpenPopup) {
		// on first open, clear old messages
		if (!ImGui::IsPopupOpen("popup-message")) {
			while (popupMessages.size() > doOpenPopup) {
				popupMessages.pop_back();
			}
		}
		doOpenPopup = 0;
		ImGui::OpenPopup("popup-message");
	}

	im::Popup("popup-message", [&]{
		printMessages(popupMessages);
		bool close = paintButtons();
		if (close) ImGui::CloseCurrentPopup();
	});
}

void ImGuiMessages::paintProgress()
{
	if (doOpenProgress) {
		doOpenProgress = false;
		ImGui::OpenPopup("popup-progress");
	}

	im::Popup("popup-progress", [&]{
		if (progressFraction >= 1.0f) {
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::TextUnformatted(progressMessage);
			if (progressFraction >= 0.0f) {
				ImGui::ProgressBar(progressFraction);
			} else {
				// unknown fraction, animate progress bar, no label
				progressTime = fmodf(progressTime + ImGui::GetIO().DeltaTime, 2.0f);
				float fraction = (progressTime < 1.0f) ? progressTime : (2.0f - progressTime);
				ImGui::ProgressBar(fraction, {}, "");
			}
		}
	});
}

void ImGuiMessages::paintOSD()
{
	auto getColors = [&](const ColorSequence& seq, float t) -> std::optional<Colors> {
		for (auto i : xrange(seq.size() - 1)) { // TODO c++23 std::views::pairwise
			const auto& step0 = seq[i + 0];
			const auto& step1 = seq[i + 1];
			if (t < step1.start) {
				PixelOperations p;
				auto x = int(256.0f * (t / step1.start));
				auto tCol = p.lerp(step0.colors.text,       step1.colors.text,       x);
				auto bCol = p.lerp(step0.colors.background, step1.colors.background, x);
				return Colors{tCol, bCol};
			}
			t -= step1.start;
		}
		return {};
	};

	const auto& style = ImGui::GetStyle();
	gl::vec2 offset = style.FramePadding;
	auto* mainViewPort = ImGui::GetMainViewport();

	struct DrawInfo {
		// clang workaround:
		DrawInfo(const std::string& m, gl::vec2 s, float y, uint32_t t, uint32_t b)
			: message(m), boxSize(s), yPos(y), textCol(t), bgCol(b) {}

		std::string message;
		gl::vec2 boxSize;
		float yPos;
		uint32_t textCol, bgCol;
	};
	std::vector<DrawInfo> drawInfo;
	float y = 0.0f;
	float width = 0.0f;
	//float wrapWidth = mainViewPort->WorkSize.x - 2.0f * offset[0]; // TODO wrap ?
	float delta = ImGui::GetIO().DeltaTime;
	std::erase_if(osdMessages, [&](OsdMessage& message) {
		message.time += delta;
		auto colors = getColors(colorSequence[message.level], message.time);
		if (!colors) return true; // remove message

		auto& text = message.text;
		gl::vec2 textSize = ImGui::CalcTextSize(text.data(), text.data() + text.size());
		gl::vec2 boxSize = textSize + 2.0f * offset;
		drawInfo.emplace_back(text, boxSize, y, colors->text, colors->background);
		y += boxSize.y + style.ItemSpacing.y;
		width = std::max(width, boxSize.x);
		return false; // keep message
	});
	if (drawInfo.empty()) return;

	int flags = ImGuiWindowFlags_NoMove
	          | ImGuiWindowFlags_NoBackground
	          | ImGuiWindowFlags_NoSavedSettings
	          | ImGuiWindowFlags_NoDocking
	          | ImGuiWindowFlags_NoNav
	          | ImGuiWindowFlags_NoDecoration
	          | ImGuiWindowFlags_NoInputs
	          | ImGuiWindowFlags_NoFocusOnAppearing;
	ImGui::SetNextWindowViewport(mainViewPort->ID);
	ImGui::SetNextWindowPos(gl::vec2(mainViewPort->WorkPos) + gl::vec2(style.ItemSpacing));
	ImGui::SetNextWindowSize({width, y});
	im::Window("OSD messages", nullptr, flags, [&]{
		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 windowPos = ImGui::GetWindowPos();
		for (const auto& [message, boxSize, yPos, textCol, bgCol] : drawInfo) {
			gl::vec2 pos = windowPos + gl::vec2{0.0f, yPos};
			drawList->AddRectFilled(pos, pos + boxSize, bgCol);
			drawList->AddText(pos + offset, textCol, message.data(), message.data() + message.size());
		}
	});
}

void ImGuiMessages::paintLog()
{
	if (logWindow.do_raise) {
		assert(logWindow.open);
		ImGui::SetNextWindowFocus();
	}
	ImGui::SetNextWindowSize(gl::vec2{40, 14} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Message Log", logWindow, ImGuiWindowFlags_NoFocusOnAppearing, [&]{
		const auto& style = ImGui::GetStyle();
		auto buttonHeight = ImGui::GetFontSize() + 2.0f * style.FramePadding.y + style.ItemSpacing.y;
		im::Child("messages", {0.0f, -buttonHeight}, ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar, [&]{
			printMessages(allMessages, [&](std::string_view prefix, std::string_view message) {
				if (filterLog.empty()) return true;
				auto full = tmpStrCat(prefix, message);
				return ranges::all_of(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(filterLog, ' '),
					[&](auto part) { return StringOp::containsCaseInsensitive(full, part); });
			});
		});
		if (ImGui::Button("Clear")) {
			allMessages.clear();
		}
		simpleToolTip("Remove all log messages");
		ImGui::SameLine(0.0f, 30.0f);
		ImGui::TextUnformatted(ICON_IGFD_SEARCH);
		ImGui::SameLine();
		auto size = ImGui::CalcTextSize("Configure..."sv).x + 30.0f + style.WindowPadding.x;
		ImGui::SetNextItemWidth(-size);
		ImGui::InputTextWithHint("##filter", "enter search terms", &filterLog);
		ImGui::SameLine(0.0f, 30.0f);
		if (ImGui::SmallButton("Configure...")) {
			configureWindow.raise();
		}
	});
}

void ImGuiMessages::paintConfigure()
{
	ImGui::SetNextWindowSize(gl::vec2{24, 26} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Configure messages", configureWindow, [&]{
		ImGui::TextUnformatted("When a message is emitted"sv);

		auto size = ImGui::CalcTextSize("Warning"sv).x;
		im::Table("table", 4, ImGuiTableFlags_SizingFixedFit, [&]{
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_None, 0);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, size);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, size);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, size);

			if (ImGui::TableNextColumn()) { /* nothing */ }
			if (ImGui::TableNextColumn()) ImGui::TextUnformatted("Error"sv);
			if (ImGui::TableNextColumn()) ImGui::TextUnformatted("Warning"sv);
			if (ImGui::TableNextColumn()) ImGui::TextUnformatted("Info"sv);

			if (ImGui::TableNextColumn()) {
				ImGui::TextUnformatted("Show popup"sv);
			}
			ImGui::TableNextRow();
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("modal"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##modal" , level).c_str(), &popupAction[level], MODAL_POPUP);
				}
			}
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("non-modal"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##popup" , level).c_str(), &popupAction[level], POPUP);
				}
			}
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("don't show"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##noPopup" , level).c_str(), &popupAction[level], NO_POPUP);
				}
			}

			if (ImGui::TableNextColumn()) {
				ImGui::TextUnformatted("Log window"sv);
			}
			ImGui::TableNextRow();
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("open and focus"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##focus" , level).c_str(), &openLogAction[level], OPEN_LOG_FOCUS);
				}
			}
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("open without focus"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##log" , level).c_str(), &openLogAction[level], OPEN_LOG);
				}
			}
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("don't open"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##nolog" , level).c_str(), &openLogAction[level], NO_OPEN_LOG);
				}
			}

			if (ImGui::TableNextColumn()) {
				ImGui::TextUnformatted("On-screen message"sv);
			}
			ImGui::TableNextRow();
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("show"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##osd" , level).c_str(), &osdAction[level], SHOW_OSD);
				}
			}
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("don't show"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					ImGui::RadioButton(tmpStrCat("##no-osd" , level).c_str(), &osdAction[level], NO_OSD);
				}
			}
			if (ImGui::TableNextColumn()) {
				im::Indent([]{ ImGui::TextUnformatted("fade-out (seconds)"sv); });
			}
			for (auto level : {CliComm::LOGLEVEL_ERROR, CliComm::WARNING, CliComm::INFO}) {
				if (ImGui::TableNextColumn()) {
					float& d = colorSequence[level][2].start;
					if (ImGui::InputFloat(tmpStrCat("##dur", level).c_str(), &d, 0.0f, 0.0f, "%.0f", ImGuiInputTextFlags_CharsDecimal)) {
						d = std::clamp(d, 1.0f, 99.0f);
					}
				}
			}
		});
	});
}

void ImGuiMessages::log(CliComm::LogLevel level, std::string_view text, float fraction)
{
	if (level == CliComm::PROGRESS) {
		progressMessage = text;
		progressFraction = fraction;
		if (progressFraction < 1.0f) doOpenProgress = true;
		return;
	}

	Message message{level, std::string(text)};

	if (popupAction[level] == MODAL_POPUP) {
		if (modalMessages.full()) modalMessages.pop_back();
		modalMessages.push_front(message);
		doOpenModal = true;
	} else if (popupAction[level] == POPUP) {
		if (popupMessages.full()) popupMessages.pop_back();
		popupMessages.push_front(message);
		doOpenPopup = popupMessages.size();
	}

	if (openLogAction[level] == OPEN_LOG) {
		logWindow.open = true;
	} else if (openLogAction[level] == OPEN_LOG_FOCUS) {
		logWindow.raise();
	}

	if (osdAction[level] == SHOW_OSD) {
		osdMessages.emplace_back(std::string(text), 0.0f, level);
	}

	if (allMessages.full()) allMessages.pop_back();
	allMessages.push_front(std::move(message));
}

} // namespace openmsx
