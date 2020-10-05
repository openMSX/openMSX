#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include "TclObject.hh"
#include "gl_vec.hh"
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
public:
	virtual ~OSDWidget() = default;

	std::string_view getName() const { return name.getString(); }
	gl::vec2 getPos()    const { return pos; }
	gl::vec2 getRelPos() const { return relPos; }
	float    getZ()      const { return z; }

	      OSDWidget* getParent()       { return parent; }
	const OSDWidget* getParent() const { return parent; }
	const SubWidgets& getChildren() const { return subWidgets; }
	void addWidget(std::unique_ptr<OSDWidget> widget);
	void deleteWidget(OSDWidget& widget);

	virtual std::vector<std::string_view> getProperties() const;
	virtual void setProperty(Interpreter& interp,
	                         std::string_view name, const TclObject& value);
	virtual void getProperty(std::string_view name, TclObject& result) const;
	virtual float getRecursiveFadeValue() const;
	virtual std::string_view getType() const = 0;

	void invalidateRecursive();
	void paintSDLRecursive(OutputSurface& output);
	void paintGLRecursive (OutputSurface& output);

	int getScaleFactor(const OutputSurface& output) const;
	gl::vec2 transformPos(const OutputSurface& output,
	                      gl::vec2 pos, gl::vec2 relPos) const;
	void getBoundingBox(const OutputSurface& output,
	                    gl::vec2& pos, gl::vec2& size) const;
	virtual gl::vec2 getSize(const OutputSurface& output) const = 0;

	Display& getDisplay() const { return display; }

protected:
	OSDWidget(Display& display, const TclObject& name);
	void invalidateChildren();
	bool needSuppressErrors() const;

	virtual void invalidateLocal() = 0;
	virtual void paintSDL(OutputSurface& output) = 0;
	virtual void paintGL (OutputSurface& output) = 0;

private:
	gl::vec2 getMouseCoord() const;
	gl::vec2 transformReverse(const OutputSurface& output,
	                          gl::vec2 pos) const;
	void setParent(OSDWidget* parent_) { parent = parent_; }
	void resortUp  (OSDWidget* elem);
	void resortDown(OSDWidget* elem);

	/** Direct child widgets of this widget, sorted by z-coordinate.
	  */
	SubWidgets subWidgets;

	Display& display;
	OSDWidget* parent;

	TclObject name;
	gl::vec2 pos;
	gl::vec2 relPos;
	float z;
	bool scaled;
	bool clip;
	bool suppressErrors;
};

} // namespace openmsx

#endif
