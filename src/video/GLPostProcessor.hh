#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "GLUtil.hh"
#include <vector>
#include <memory>

namespace openmsx {

class GLScaler;
class MSXMotherBoard;
class Display;

/** Rasterizer using SDL.
  */
class GLPostProcessor final : public PostProcessor
{
public:
	GLPostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, const std::string& videoSource,
		unsigned maxWidth, unsigned height, bool canDoInterlace);
	~GLPostProcessor() override;

	// Layer interface:
	void paint(OutputSurface& output) override;

	[[nodiscard]] std::unique_ptr<RawFrame> rotateFrames(
		std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time) override;

protected:
	// Observer<Setting> interface:
	void update(const Setting& setting) noexcept override;

private:
	void initBuffers();
	void createRegions();
	void uploadFrame();
	void uploadBlock(unsigned srcStartY, unsigned srcEndY,
	                 unsigned lineWidth);

	void preCalcNoise(float factor);
	void drawNoise();
	void drawGlow(int glow);

	void preCalcMonitor3D(float width);
	void drawMonitor3D();

private:
	/** The currently active scaler.
	  */
	std::unique_ptr<GLScaler> currScaler;

	gl::Texture colorTex[2];
	gl::FrameBufferObject fbo[2];

	// Noise effect:
	gl::Texture noiseTextureA;
	gl::Texture noiseTextureB;
	float noiseX, noiseY;

	struct TextureData {
		gl::ColorTexture tex;
		gl::PixelBuffer<unsigned> pbo;
		[[nodiscard]] unsigned width() const { return tex.getWidth(); }
	};
	std::vector<TextureData> textures;

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
	gl::BufferObject vbo;
	gl::BufferObject stretchVBO;

	bool storedFrame;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
