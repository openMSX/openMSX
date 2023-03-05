#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "ImGuiMarkdown.hh"
#include "EventListener.hh"
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

class ImGuiLayer final : public Layer, private EventListener
{
public:
	ImGuiLayer(Reactor& reactor);
	~ImGuiLayer() override;

private:
	// Layer
	void paint(OutputSurface& output) override;

	// EventListener
	int signalEvent(const Event& event) override;

	void mediaMenu(MSXMotherBoard* motherBoard);
	void connectorsMenu(MSXMotherBoard* motherBoard);
	void saveStateMenu(MSXMotherBoard* motherBoard);
	void settingsMenu();
	void soundChipSettings(MSXMotherBoard& motherBoard);
	void channelSettings(MSXMotherBoard& motherBoard, const std::string& name, bool* enabled);
	void drawReverseBar(MSXMotherBoard& motherBoard);
	void drawIcons();
	void drawConfigureIcons();
	void setDefaultIcons();
	void loadIcons();
	void debuggableMenu(MSXMotherBoard* motherBoard);
	void disassembly(MSXMotherBoard& motherBoard);
	void registers(MSXMotherBoard& motherBoard);
	void flags(MSXMotherBoard& motherBoard);
	void renderBitmap(std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette16,
	                  int mode, int lines, int page, uint32_t* output);
	void bitmapViewer(MSXMotherBoard& motherBoard);
	void helpMenu();
	void drawHelpWindow();

	std::optional<TclObject> execute(TclObject command);
	void executeDelayed(TclObject command);
	void selectFileCommand(const std::string& title, std::string filters, TclObject command);
	void selectFile(const std::string& title, std::string filters,
                        std::function<void(const std::string&)> callback);

public:
	// TODO dynamic font loading in ImGui is technically possible, though not trivial
	// So for now pre-load all the fonts we'll need.
	//   see https://github.com/ocornut/imgui/issues/2311
	ImFont* vera13 = nullptr;
	ImFont* veraBold13 = nullptr;
	ImFont* veraBold16 = nullptr;
	ImFont* veraItalic13 = nullptr;
	ImFont* veraBoldItalic13 = nullptr;

private:
	Reactor& reactor;
	Interpreter& interp;
	std::vector<TclObject> commandQueue;

	std::map<std::string, std::unique_ptr<DebuggableEditor>> debuggables;
	std::map<std::string, bool> channels;

	std::map<std::string, std::string> lastPath;
	std::string lastFileDialog;
	std::function<void(const std::string&)> openFileCallback;

	bool showHelpWindow = false;
	ImGuiMarkdown markdown;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;

	bool showReverseBar = true;
	bool reverseHideTitle = false;
	bool reverseFadeOut = false;
	float reverseAlpha = 1.0f;

	bool showIcons = true;
	bool iconsHideTitle = false;
	bool iconsAllowMove = true;
	int iconsHorizontal = 1; // 0=vertical, 1=horizontal
	struct IconInfo {
		IconInfo(TclObject expr_, std::string on_, std::string off_, bool fade_)
			: expr(expr_), on(std::move(on_)), off(std::move(off_)), fade(fade_) {}
		struct Icon {
			Icon(std::string filename_) : filename(std::move(filename_)) {}
			std::string filename;
			gl::Texture tex{gl::Null{}};
			gl::ivec2 size;
		};
		TclObject expr;
		Icon on, off;
		float time = 0.0f;
		bool lastState = true;
		bool enable = true;
		bool fade;
	};
	std::vector<IconInfo> iconInfo;
	gl::ivec2 iconsTotalSize;
	gl::ivec2 iconsMaxSize;
	float iconsFadeDuration = 5.0f;
	float iconsFadeDelay = 5.0f;
	int iconsNumEnabled = 0;
	bool iconInfoDirty = false;
	bool showConfigureIcons = false;

	bool showDisassembly = false;
	bool syncDisassemblyWithPC = false;

	bool showRegisters = false;
	bool showFlags = false;
	bool showUndocumentedFlags = false;
	int flagsLayout = 1; // 0=horizontal 1=vertical

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
