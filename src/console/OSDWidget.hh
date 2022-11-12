#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include "TclObject.hh"
#include "gl_vec.hh"
#include <array>
#include <span>
#include <string_view>
#include <vector>
#include <memory>

namespace openmsx {

class Display;
class OutputSurface;
class Interpreter;

class OSDWidget
{
	using SubWidgets = std::vector<std::unique_ptr<OSDWidget>>;

protected:
	static constexpr auto widgetProperties = [] {
		using namespace std::literals;
		return std::array{
			"-type"sv, "-x"sv, "-y"sv, "-z"sv, "-relx"sv, "-rely"sv, "-scaled"sv,
			"-clip"sv, "-mousecoord"sv, "-suppressErrors"sv,
		};
	}();

public:
	virtual ~OSDWidget() = default;

	[[nodiscard]] std::string_view getName() const { return name.getString(); }
	[[nodiscard]] gl::vec2 getPos()    const { return pos; }
	[[nodiscard]] gl::vec2 getRelPos() const { return relPos; }
	[[nodiscard]] float    getZ()      const { return z; }

	[[nodiscard]]       OSDWidget* getParent()       { return parent; }
	[[nodiscard]] const OSDWidget* getParent() const { return parent; }
	[[nodiscard]] const SubWidgets& getChildren() const { return subWidgets; }
	void addWidget(std::unique_ptr<OSDWidget> widget);
	void deleteWidget(OSDWidget& widget);

	[[nodiscard]] virtual std::span<const std::string_view> getProperties() const {
		return widgetProperties;
	}
	virtual void setProperty(Interpreter& interp,
	                         std::string_view name, const TclObject& value);
	virtual void getProperty(std::string_view name, TclObject& result) const;
	[[nodiscard]] virtual float getRecursiveFadeValue() const;
	[[nodiscard]] virtual bool isRecursiveFading() const = 0;
	[[nodiscard]] virtual std::string_view getType() const = 0;

	void invalidateRecursive();
	void paintSDLRecursive(OutputSurface& output);
	void paintGLRecursive (OutputSurface& output);

	[[nodiscard]] int getScaleFactor(const OutputSurface& output) const;
	[[nodiscard]] gl::vec2 transformPos(const OutputSurface& output,
	                                    gl::vec2 pos, gl::vec2 relPos) const;
	void getBoundingBox(const OutputSurface& output,
	                    gl::vec2& pos, gl::vec2& size) const;
	[[nodiscard]] virtual gl::vec2 getSize(const OutputSurface& output) const = 0;

	// Is visible? Or may become visible (fading-in).
	[[nodiscard]] virtual bool isVisible() const = 0;

	[[nodiscard]] Display& getDisplay() const { return display; }

protected:
	OSDWidget(Display& display, TclObject name);
	void invalidateChildren();
	[[nodiscard]] bool needSuppressErrors() const;

	virtual void invalidateLocal() = 0;
	virtual void paintSDL(OutputSurface& output) = 0;
	virtual void paintGL (OutputSurface& output) = 0;

private:
	[[nodiscard]] gl::vec2 getMouseCoord() const;
	[[nodiscard]] gl::vec2 transformReverse(const OutputSurface& output,
	                                        gl::vec2 pos) const;
	void setParent(OSDWidget* parent_) { parent = parent_; }
	void resortUp  (OSDWidget* elem);
	void resortDown(OSDWidget* elem);

private:
	/** Direct child widgets of this widget, sorted by z-coordinate.
	  */
	SubWidgets subWidgets;

	Display& display;
	OSDWidget* parent = nullptr;

	TclObject name;
	gl::vec2 pos;
	gl::vec2 relPos;
	float z = 0.0f;
	bool scaled = false;
	bool clip = false;
	bool suppressErrors = false;
};

} // namespace openmsx

#endif
