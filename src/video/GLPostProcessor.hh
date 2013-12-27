#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "GLUtil.hh"
#include <map>
#include <vector>
#include <memory>

namespace openmsx {

class GLScaler;
class MSXMotherBoard;
class Display;

/** Rasterizer using SDL.
  */
class GLPostProcessor : public PostProcessor
{
public:
	GLPostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, const std::string& videoSource,
		unsigned maxWidth, unsigned height, bool canDoInterlace);
	virtual ~GLPostProcessor();

	// Layer interface:
	virtual void paint(OutputSurface& output);

	virtual std::unique_ptr<RawFrame> rotateFrames(
		std::unique_ptr<RawFrame> finishedFrame, FrameSource::FieldType field,
		EmuTime::param time);

protected:
	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

private:
	void createRegions();
	void uploadFrame();
	void uploadBlock(unsigned srcStartY, unsigned srcEndY,
	                 unsigned lineWidth);

	void preCalcNoise(float factor);
	void drawNoise();
	void drawGlow(int glow);

	void preCalc3DDisplayList(double width);

	/** The currently active scaler.
	  */
	std::unique_ptr<GLScaler> currScaler;

	Texture colorTex[2];
	FrameBufferObject fbo[2];

	// Noise effect:
	LuminanceTexture noiseTextureA;
	LuminanceTexture noiseTextureB;
	double noiseX;
	double noiseY;

	struct TextureData {
		TextureData();
		TextureData(TextureData&& rhs);

		ColorTexture tex;
		PixelBuffer<unsigned> pbo;
	};
	std::map<unsigned, TextureData> textures;

	ColorTexture superImposeTex;

	struct Region {
		Region(unsigned srcStartY_, unsigned srcEndY_,
		       unsigned dstStartY_, unsigned dstEndY_,
		       unsigned lineWidth_)
			: srcStartY(srcStartY_)
			, srcEndY(srcEndY_)
			, dstStartY(dstStartY_)
			, dstEndY(dstEndY_)
			, lineWidth(lineWidth_) {}
		unsigned srcStartY;
		unsigned srcEndY;
		unsigned dstStartY;
		unsigned dstEndY;
		unsigned lineWidth;
	};
	std::vector<Region> regions;

	unsigned height;
	unsigned frameCounter;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm;

	GLuint monitor3DList;

	bool storedFrame;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
