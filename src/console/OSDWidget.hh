// $Id$

#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include "openmsx.hh"
#include <string>
#include <vector>
#include <memory>
#include <set>

namespace openmsx {

class OutputSurface;

class OSDWidget
{
public:
	virtual ~OSDWidget();

	const std::string& getName() const;
	int    getX()    const { return x; }
	int    getY()    const { return y; }
	int    getZ()    const { return z; }
	double getRelX() const { return relx; }
	double getRelY() const { return rely; }

	OSDWidget* getParent();
	const OSDWidget* getParent() const;
	OSDWidget* findSubWidget(const std::string& name);
	const OSDWidget* findSubWidget(const std::string& name) const;
	void addWidget(std::auto_ptr<OSDWidget> widget);
	void deleteWidget(OSDWidget& widget);

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const = 0;

	void invalidateRecursive();
	void paintSDLRecursive(OutputSurface& output);
	void paintGLRecursive (OutputSurface& output);

	int getScaleFactor(const OutputSurface& surface) const;
	void transformXY(const OutputSurface& output,
	                 int x, int y, double relx, double rely,
	                 int& outx, int& outy) const;

protected:
	OSDWidget(const std::string& name);
	void invalidateChildren();

	virtual void invalidateLocal() = 0;
	virtual void getWidthHeight(const OutputSurface& output,
	                            int& width, int& height) const = 0;
	virtual void paintSDL(OutputSurface& output) = 0;
	virtual void paintGL (OutputSurface& output) = 0;

private:
	void setParent(OSDWidget* parent);
	void resort();

	void listWidgetNames(const std::string& parentName,
	                     std::set<std::string>& result) const;
	friend class OSDCommand;

	typedef std::vector<OSDWidget*> SubWidgets;
	SubWidgets subWidgets;
	OSDWidget* parent;

	const std::string name;
	double relx, rely;
	int x, y, z;
	bool scaled;
};

} // namespace openmsx

#endif
