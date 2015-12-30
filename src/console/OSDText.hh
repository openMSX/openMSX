#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDImageBasedWidget.hh"
#include "TTFFont.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class OSDText final : public OSDImageBasedWidget
{
public:
	OSDText(OSDGUI& gui, const TclObject& name);

	std::vector<string_ref> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 string_ref name, const TclObject& value) override;
	void getProperty(string_ref name, TclObject& result) const override;
	string_ref getType() const override;

private:
	void invalidateLocal() override;
	gl::vec2 getSize(const OutputRectangle& output) const override;
	byte getFadedAlpha() const override;
	std::unique_ptr<BaseImage> createSDL(OutputRectangle& output) override;
	std::unique_ptr<BaseImage> createGL (OutputRectangle& output) override;
	template <typename IMAGE> std::unique_ptr<BaseImage> create(
		OutputRectangle& output);

	template<typename FindSplitPointFunc, typename CantSplitFunc>
	size_t split(const std::string& line, unsigned maxWidth,
		FindSplitPointFunc findSplitPoint, CantSplitFunc cantSplit,
		bool removeTrailingSpaces) const;
	size_t splitAtChar(const std::string& line, unsigned maxWidth) const;
	size_t splitAtWord(const std::string& line, unsigned maxWidth) const;
	std::string getCharWrappedText(const std::string& text, unsigned maxWidth) const;
	std::string getWordWrappedText(const std::string& text, unsigned maxWidth) const;

	gl::vec2 getRenderedSize() const;

	enum WrapMode { NONE, WORD, CHAR };

	std::string text;
	std::string fontfile;
	TTFFont font;
	int size;
	WrapMode wrapMode;
	float wrapw, wraprelw;

	friend struct SplitAtChar;
};

} // namespace openmsx

#endif
