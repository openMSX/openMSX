#ifndef IMGUI_RASTER_VIEWER_HH
#define IMGUI_RASTER_VIEWER_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "MemBuffer.hh"

namespace openmsx {

class ImGuiRasterViewer final : public ImGuiPart
{
public:
	ImGuiRasterViewer(ImGuiManager& manager_);

	[[nodiscard]] zstring_view iniName() const override { return "Raster Viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void initPixelBuffer(size_t lines);

public:
	bool show = true; // TODO false

	MemBuffer<uint32_t> pixelBuffer;
	gl::Texture viewTex{gl::Null{}};
	//gl::Texture gridTex{gl::Null{}};
	gl::Texture checkerTex{gl::Null{}};

private:
	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",           &ImGuiRasterViewer::show},
	};
};

} // namespace openmsx

#endif
