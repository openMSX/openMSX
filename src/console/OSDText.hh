// $Id$

#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDImageBasedWidget.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class TTFFont;

class OSDText : public OSDImageBasedWidget
{
public:
	OSDText(const OSDGUI& gui, const std::string& name);
	~OSDText();

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(string_ref name, const TclObject& value);
	virtual void getProperty(string_ref name, TclObject& result) const;
	virtual string_ref getType() const;

private:
	virtual void invalidateLocal();
	virtual void getWidthHeight(const OutputRectangle& output,
	                            double& width, double& height) const;
	virtual byte getFadedAlpha() const;
	virtual std::unique_ptr<BaseImage> createSDL(OutputRectangle& output);
	virtual std::unique_ptr<BaseImage> createGL (OutputRectangle& output);
	template <typename IMAGE> std::unique_ptr<BaseImage> create(
		OutputRectangle& output);

	template<typename FindSplitPointFunc, typename CantSplitFunc>
	unsigned split(const std::string& line, unsigned maxWidth,
		FindSplitPointFunc findSplitPoint, CantSplitFunc cantSplit,
		bool removeTrailingSpaces) const;
	unsigned splitAtChar(const std::string& line, unsigned maxWidth) const;
	unsigned splitAtWord(const std::string& line, unsigned maxWidth) const;
	std::string getCharWrappedText(const std::string& text, unsigned maxWidth) const;
	std::string getWordWrappedText(const std::string& text, unsigned maxWidth) const;

	void getRenderedSize(double& outX, double& outY) const;

	enum WrapMode { NONE, WORD, CHAR };

	std::string text;
	std::string fontfile;
	std::unique_ptr<TTFFont> font;
	int size;
	WrapMode wrapMode;
	double wrapw, wraprelw;

	friend struct SplitAtChar;
};

} // namespace openmsx

#endif
