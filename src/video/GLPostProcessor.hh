// $Id$

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
class CommandController;
class Display;
class VisibleSurface;
class Texture;
class FrameBufferObject;

/** Rasterizer using SDL.
  */
class GLPostProcessor : public PostProcessor
{
public:
	GLPostProcessor(
		CommandController& commandController, Display& display,
		VisibleSurface& screen, VideoSource videoSource,
		unsigned maxWidth, unsigned height);
	virtual ~GLPostProcessor();

	// Layer interface:
	virtual void paint();

	virtual RawFrame* rotateFrames(
		RawFrame* finishedFrame, FrameSource::FieldType field,
		const EmuTime& time);

protected:
	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

private:
	void createRegions();
	void uploadFrame();
	void uploadBlock(unsigned srcStartY, unsigned srcEndY,
	                 unsigned lineWidth);

	void preCalcNoise(double factor);
	void drawNoise();
	void drawGlow(int glow);

	void preCalc3DDisplayList(double width);

	/** The currently active scaler.
	  */
	std::auto_ptr<GLScaler> currScaler;

	std::auto_ptr<Texture> colorTex[2];
	std::auto_ptr<FrameBufferObject> fbo[2];

	// Noise effect:
	LuminanceTexture noiseTextureA;
	LuminanceTexture noiseTextureB;
	double noiseX;
	double noiseY;

	struct TextureData {
		ColourTexture* tex;
		PixelBuffer<unsigned>* pbo;
	};
	typedef std::map<unsigned, TextureData> Textures;
	Textures textures;

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
	typedef std::vector<Region> Regions;
	Regions regions;

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
