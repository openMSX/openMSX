#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDImageBasedWidget.hh"
#include "TTFFont.hh"
#include <memory>

namespace openmsx {

class OSDText final : public OSDImageBasedWidget
{
public:
	OSDText(Display& display, const TclObject& name);

	std::vector<std::string_view> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 std::string_view name, const TclObject& value) override;
	void getProperty(std::string_view name, TclObject& result) const override;
	std::string_view getType() const override;

private:
	void invalidateLocal() override;
	gl::vec2 getSize(const OutputSurface& output) const override;
	uint8_t getFadedAlpha() const override;
	std::unique_ptr<BaseImage> createSDL(OutputSurface& output) override;
	std::unique_ptr<BaseImage> createGL (OutputSurface& output) override;
	template <typename IMAGE> std::unique_ptr<BaseImage> create(
		OutputSurface& output);

	template<typename FindSplitPointFunc, typename CantSplitFunc>
	size_t split(const std::string& line, unsigned maxWidth,
		FindSplitPointFunc findSplitPoint, CantSplitFunc cantSplit,
		bool removeTrailingSpaces) const;
	size_t splitAtChar(const std::string& line, unsigned maxWidth) const;
	size_t splitAtWord(const std::string& line, unsigned maxWidth) const;
	std::string getCharWrappedText(const std::string& txt, unsigned maxWidth) const;
	std::string getWordWrappedText(const std::string& txt, unsigned maxWidth) const;

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
