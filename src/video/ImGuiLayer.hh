#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "GLUtil.hh"
#include "Layer.hh"
#include "TclObject.hh"
#include "gl_vec.hh"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace openmsx {

class DebuggableEditor;
class Interpreter;
class MSXMotherBoard;
class Reactor;
class VDPVRAM;

class ImGuiLayer final : public Layer
{
public:
	ImGuiLayer(Reactor& reactor);
	~ImGuiLayer() override;

private:
	// Layer
	void paint(OutputSurface& output) override;

	void mediaMenu(MSXMotherBoard* motherBoard);
	void connectorsMenu(MSXMotherBoard* motherBoard);
	void saveStateMenu(MSXMotherBoard* motherBoard);
	void settingsMenu();
	void soundChipSettings(MSXMotherBoard* motherBoard);
	void channelSettings(MSXMotherBoard* motherBoard, const std::string& name, bool* enabled);
	void debuggableMenu(MSXMotherBoard* motherBoard);
	void renderBitmap(std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette16,
	                  int mode, int lines, int page, uint32_t* output);
	void bitmapViewer(MSXMotherBoard* motherBoard);

	std::optional<TclObject> execute(TclObject command);
	void selectFileCommand(const std::string& title, const char* filters, TclObject command);

private:
	Reactor& reactor;
	Interpreter& interp;
	std::map<std::string, std::unique_ptr<DebuggableEditor>> debuggables;
	std::map<std::string, bool> channels;

	std::function<void(const std::string&)> openFileCallback;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;

	bool showBitmapViewer = false;
	int bitmapManual = 0; // 0 -> use VDP settings, 1 -> use manual settings
	enum BitmapScrnMode : int { SCR5, SCR6, SCR7, SCR8, SCR11, SCR12, OTHER };
	int bitmapScrnMode = SCR5;
	int bitmapPage = 0; // 0-3 or 0-1 depending on screen mode   TODO extended VRAM
	int bitmapLines = 1; // 0->192, 1->212, 2->256
	int bitmapColor0 = 16; // 0..15, 16->no replacement
	int bitmapPalette = 0; // 0->actual, 1->fixed, 2->custom
	int bitmapZoom = 1; // 0->1x, 1->2x, ..., 7->8x
	bool bitmapGrid = true;
	gl::vec4 bitmapGridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA
	gl::Texture bitmapTex{false, false}; // no interpolation, no wrapping
	gl::Texture bitmapGridTex{false, true}; // no interpolation, with wrapping

	bool showDemoWindow = false;
	bool showSoundChipSettings = false;
	bool first = true;
};

} // namespace openmsx

#endif
