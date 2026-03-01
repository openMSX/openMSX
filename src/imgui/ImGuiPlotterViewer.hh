#ifndef IMGUI_PLOTTER_VIEWER_HH
#define IMGUI_PLOTTER_VIEWER_HH

#include "GLUtil.hh"
#include "ImGuiPart.hh"
#include <optional>

namespace openmsx {

class ImGuiPlotterViewer final : public ImGuiPart {
public:
  explicit ImGuiPlotterViewer(ImGuiManager &manager);

  [[nodiscard]] zstring_view iniName() const override {
    return "Plotter viewer";
  }
  void save(ImGuiTextBuffer &buf) override;
  void loadLine(std::string_view name, zstring_view value) override;
  void paint(MSXMotherBoard *motherBoard) override;

  bool show = false;

private:
  void updateTexture(class Paper *paper);

  std::optional<gl::ColorTexture> texture;
  uint64_t lastGeneration = uint64_t(-1);
  std::vector<uint8_t> downsampledBuf;

  static constexpr auto persistentElements = std::tuple{
      PersistentElement{"show", &ImGuiPlotterViewer::show},
  };
};

} // namespace openmsx

#endif
