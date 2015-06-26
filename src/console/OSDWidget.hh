#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include "gl_vec.hh"
#include "hash_set.hh"
#include "string_ref.hh"
#include "xxhash.hh"
#include <vector>
#include <memory>

namespace openmsx {

class OutputRectangle;
class OutputSurface;
class TclObject;
class Interpreter;

class OSDWidget
{
public:
	virtual ~OSDWidget();

	const std::string& getName() const { return name; }
	gl::vec2 getPos()    const { return pos; }
	gl::vec2 getRelPos() const { return relPos; }
	float    getZ()      const { return z; }

	      OSDWidget* getParent()       { return parent; }
	const OSDWidget* getParent() const { return parent; }
	OSDWidget* findSubWidget(string_ref name);
	const OSDWidget* findSubWidget(string_ref name) const;
	void addWidget(std::unique_ptr<OSDWidget> widget);
	void deleteWidget(OSDWidget& widget);

	virtual std::vector<string_ref> getProperties() const;
	virtual void setProperty(Interpreter& interp,
	                         string_ref name, const TclObject& value);
	virtual void getProperty(string_ref name, TclObject& result) const;
	virtual float getRecursiveFadeValue() const;
	virtual string_ref getType() const = 0;

	void invalidateRecursive();
	void paintSDLRecursive(OutputSurface& output);
	void paintGLRecursive (OutputSurface& output);

	int getScaleFactor(const OutputRectangle& surface) const;
	gl::vec2 transformPos(const OutputRectangle& output,
	                      gl::vec2 pos, gl::vec2 relPos) const;
	void getBoundingBox(const OutputRectangle& output,
	                    gl::ivec2& pos, gl::ivec2& size);
	virtual gl::vec2 getSize(const OutputRectangle& output) const = 0;

	// for OSDGUI::OSDCommand
	void listWidgetNames(const std::string& parentName,
	                     std::vector<std::string>& result) const;

protected:
	explicit OSDWidget(const std::string& name);
	void invalidateChildren();
	bool needSuppressErrors() const;

	virtual void invalidateLocal() = 0;
	virtual void paintSDL(OutputSurface& output) = 0;
	virtual void paintGL (OutputSurface& output) = 0;

private:
	struct NameFromWidget {
		const std::string& operator()(const OSDWidget* w) const {
			return w->getName();
		}
	};

	gl::vec2 getMouseCoord() const;
	gl::vec2 transformReverse(const OutputRectangle& output,
	                          gl::vec2 pos) const;
	void setParent(OSDWidget* parent_) { parent = parent_; }
	void resortUp  (OSDWidget* elem);
	void resortDown(OSDWidget* elem);

	/** Direct child widgets of this widget, sorted by z-coordinate.
	  */
	std::vector<std::unique_ptr<OSDWidget>> subWidgets;

	/** Contains the same widgets as "subWidgets", but stored with their name
	  * the key, so lookup by name is fast.
	  */
	hash_set<OSDWidget*, NameFromWidget, XXHasher> subWidgetsMap;
	//TODO make one global hash_set??

	OSDWidget* parent;

	const std::string name;
	gl::vec2 pos;
	gl::vec2 relPos;
	float z;
	bool scaled;
	bool clip;
	bool suppressErrors;
};

} // namespace openmsx

#endif
