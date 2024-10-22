#ifndef IMGUI_REVERSE_BAR_HH
#define IMGUI_REVERSE_BAR_HH

#include "ImGuiAdjust.hh"
#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "TclObject.hh"

#include <string>
#include <filesystem>

namespace openmsx {

class ImGuiReverseBar final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	[[nodiscard]] zstring_view iniName() const override { return "reverse bar"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool showReverseBar = true;

private:
	std::string saveStateName;
	std::string saveReplayName;
	bool saveStateOpen = false;
	bool loadStateOpen = false;
	struct StateNames {
		StateNames(std::string n, std::filesystem::file_time_type t) // workaround, needed for clang, not gcc or msvc
			: name(std::move(n)), ftime(std::move(t)) {} // fixed in clang-16
		std::string name;
		std::filesystem::file_time_type ftime;
	};
	std::vector<StateNames> stateNames;
	bool saveReplayOpen = false;
	bool loadReplayOpen = false;
	struct ReplayNames {
		ReplayNames(std::string f, std::string d, std::filesystem::file_time_type t) // workaround, needed for clang, not gcc or msvc
			: fullName(std::move(f)), displayName(std::move(d)), ftime(std::move(t)) {} // fixed in clang-16
		std::string fullName;
		std::string displayName;
		std::filesystem::file_time_type ftime;
	};
	std::vector<ReplayNames> replayNames;
	TclObject confirmCmd;
	std::string confirmText;

	bool reverseHideTitle = true;
	bool reverseFadeOut = true;
	bool reverseAllowMove = false;
	float reverseAlpha = 1.0f;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;

	AdjustWindowInMainViewPort adjust;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",      &ImGuiReverseBar::showReverseBar},
		PersistentElement{"hideTitle", &ImGuiReverseBar::reverseHideTitle},
		PersistentElement{"fadeOut",   &ImGuiReverseBar::reverseFadeOut},
		PersistentElement{"allowMove", &ImGuiReverseBar::reverseAllowMove}
		// manually handle "adjust"
	};
};

} // namespace openmsx

#endif
