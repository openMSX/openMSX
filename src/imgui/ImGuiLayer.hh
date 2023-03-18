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
class ImGuiManager;
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
	void drawRegisters(MSXMotherBoard& motherBoard);
	void drawFlags(MSXMotherBoard& motherBoard);
	void drawStack(MSXMotherBoard& motherBoard);
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

	void syncPersistentData(ImGuiManager& imGui);

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

	std::string lastFileDialog;
	std::function<void(const std::string&)> openFileCallback;

	bool showHelpWindow = false;
	ImGuiMarkdown markdown;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;

	bool* showReverseBar;
	bool* reverseHideTitle;
	bool* reverseFadeOut;
	bool* reverseAllowMove;
	float reverseAlpha = 1.0f;

	bool* showIcons;
	bool* iconsHideTitle;
	bool* iconsAllowMove;
	int* iconsHorizontal; // 0=vertical, 1=horizontal
	float* iconsFadeDuration;
	float* iconsFadeDelay;
	bool* showConfigureIcons;
	struct IconInfo {
		IconInfo(TclObject expr_, std::string on_, std::string off_, bool fade_, bool enable_ = true)
			: expr(expr_), on(std::move(on_)), off(std::move(off_)), enable(enable_), fade(fade_) {}
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
	int iconsNumEnabled = 0;
	bool iconInfoDirty = false;

	bool* showDisassembly;
	bool syncDisassemblyWithPC = false;

	bool* showRegisters;
	bool* showStack;
	bool* showFlags;
	bool* showXYFlags;
	int* flagsLayout;

	bool* showBitmapViewer;
	int* bitmapManual; // 0 -> use VDP settings, 1 -> use manual settings
	enum BitmapScrnMode : int { SCR5, SCR6, SCR7, SCR8, SCR11, SCR12, OTHER };
	int* bitmapScrnMode;
	int* bitmapPage; // 0-3 or 0-1 depending on screen mode   TODO extended VRAM
	int* bitmapLines; // 0->192, 1->212, 2->256
	int* bitmapColor0; // 0..15, 16->no replacement
	int* bitmapPalette; // 0->actual, 1->fixed, 2->custom
	int* bitmapZoom; // 0->1x, 1->2x, ..., 7->8x
	bool* bitmapGrid;
	gl::vec4* bitmapGridColor; // RGBA
	gl::Texture bitmapTex{false, false}; // no interpolation, no wrapping
	gl::Texture bitmapGridTex{false, true}; // no interpolation, with wrapping

	bool showDemoWindow = false;
	bool* showSoundChipSettings;
	bool first = true;
};

} // namespace openmsx

#endif
