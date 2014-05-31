#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "GLUtil.hh"
#include <utility>
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
		std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time);

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

	void preCalcMonitor3D(float width);
	void drawMonitor3D();

	/** The currently active scaler.
	  */
	std::unique_ptr<GLScaler> currScaler;

	gl::Texture colorTex[2];
	gl::FrameBufferObject fbo[2];

	// Noise effect:
	gl::LuminanceTexture noiseTextureA;
	gl::LuminanceTexture noiseTextureB;
	double noiseX;
	double noiseY;

	struct TextureData {
		TextureData();
		TextureData(TextureData&& rhs)
#if !defined(_MSC_VER)
			// Visual C++ doesn't support 'noexcept' yet. However,
			// unless there is a move operation that never throws,
			// std::vector may require a copy constructor, and in
			// fact that happens with LLVM's libc++.
			noexcept
#endif
			;

		gl::ColorTexture tex;
		gl::PixelBuffer<unsigned> pbo;
	};
	std::vector<std::pair<unsigned, TextureData>> textures;

	gl::ColorTexture superImposeTex;

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

	gl::ShaderProgram monitor3DProg;
	gl::BufferObject arrayBuffer;
	gl::BufferObject elementbuffer;

	gl::ShaderProgram texProg;

	gl::ShaderProgram glowProg;
	GLint glowAlphaLoc;

	bool storedFrame;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
