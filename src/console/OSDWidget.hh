#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include "TclObject.hh"
#include "gl_vec.hh"
#include "hash_set.hh"
#include "string_view.hh"
#include "xxhash.hh"
#include <vector>
#include <memory>

namespace openmsx {

class Display;
class OutputRectangle;
class OutputSurface;
class TclObject;
class Interpreter;

class OSDWidget
{
	using SubWidgets = std::vector<std::unique_ptr<OSDWidget>>;
public:
	virtual ~OSDWidget() = default;

	string_view getName() const { return name.getString(); }
	gl::vec2 getPos()    const { return pos; }
	gl::vec2 getRelPos() const { return relPos; }
	float    getZ()      const { return z; }

	      OSDWidget* getParent()       { return parent; }
	const OSDWidget* getParent() const { return parent; }
	const SubWidgets& getChildren() const { return subWidgets; }
	void addWidget(std::unique_ptr<OSDWidget> widget);
	void deleteWidget(OSDWidget& widget);

	virtual std::vector<string_view> getProperties() const;
	virtual void setProperty(Interpreter& interp,
	                         string_view name, const TclObject& value);
	virtual void getProperty(string_view name, TclObject& result) const;
	virtual float getRecursiveFadeValue() const;
	virtual string_view getType() const = 0;

	void invalidateRecursive();
	void paintSDLRecursive(OutputSurface& output);
	void paintGLRecursive (OutputSurface& output);

	int getScaleFactor(const OutputRectangle& output) const;
	gl::vec2 transformPos(const OutputRectangle& output,
	                      gl::vec2 pos, gl::vec2 relPos) const;
	void getBoundingBox(const OutputRectangle& output,
	                    gl::ivec2& pos, gl::ivec2& size);
	virtual gl::vec2 getSize(const OutputRectangle& output) const = 0;

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
	gl::vec2 transformReverse(const OutputRectangle& output,
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
