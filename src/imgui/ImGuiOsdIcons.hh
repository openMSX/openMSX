#ifndef IMGUI_OSD_ICONS_HH
#define IMGUI_OSD_ICONS_HH

#include "ImGuiAdjust.hh"
#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "TclObject.hh"
#include "gl_vec.hh"

#include <vector>

namespace openmsx {

class ImGuiOsdIcons final : public ImGuiPart
{
public:
	explicit ImGuiOsdIcons(ImGuiManager& manager_);

	[[nodiscard]] zstring_view iniName() const override { return "OSD icons"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void setDefaultIcons();
	void loadIcons();
	void paintConfigureIcons();

public:
	bool showIcons = true;
	bool showConfigureIcons = false;

private:
	bool hideTitle = true;
	bool allowMove = false;
	float fadeDuration = 5.0f;
	float fadeDelay = 5.0f;

	struct IconInfo {
		IconInfo() = default;
		IconInfo(TclObject expr_, std::string on_, std::string off_, bool fade_)
			: expr(expr_), on(std::move(on_)), off(std::move(off_)), fade(fade_) {}
		struct Icon {
			Icon() = default;
			explicit Icon(std::string filename_) : filename(std::move(filename_)) {}
			std::string filename;
			gl::Texture tex{gl::Null{}};
			gl::ivec2 size;
		};
		TclObject expr;
		Icon on, off;
		float time = 0.0f;
		bool lastState = true;
		bool enable = true;
		bool fade = false;
	};
	std::vector<IconInfo> iconInfo;
	gl::vec2 maxIconSize;
	int numIcons = 0; // number of enabled icons
	bool iconInfoDirty = true;

	AdjustWindowInMainViewPort adjust;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",         &ImGuiOsdIcons::showIcons},
		PersistentElement{"hideTitle",    &ImGuiOsdIcons::hideTitle},
		PersistentElement{"allowMove",    &ImGuiOsdIcons::allowMove},
		PersistentElement{"fadeDuration", &ImGuiOsdIcons::fadeDuration},
		PersistentElement{"fadeDelay",    &ImGuiOsdIcons::fadeDelay},
		PersistentElement{"showConfig",   &ImGuiOsdIcons::showConfigureIcons}
		// manually handle "icon.xxx", "adjust"
	};
};

} // namespace openmsx

#endif
