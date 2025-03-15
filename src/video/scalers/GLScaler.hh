#ifndef GLSCALER_HH
#define GLSCALER_HH

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <array>
#include <string>

namespace openmsx {

class FrameSource;

/** Abstract base class for OpenGL scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
class GLScaler
{
public:
	virtual ~GLScaler() = default;

	/** Setup scaler.
	  * Must be called once per frame before calling scaleImage() (possibly
	  * multiple times).
	  */
	void setup(bool superImpose, gl::ivec2 dstSize);

	/** Scales the image in the given area, which must consist of lines which
	  * are all equally wide.
	  * Scaling factor depends on the concrete scaler.
	  * @param src Source: texture containing the frame to be scaled.
	  * @param superImpose Texture containing the to-be-superimposed image (can be nullptr).
	  * @param srcStartY Y-coordinate of the top source line (inclusive).
	  * @param srcEndY Y-coordinate of the bottom source line (exclusive).
	  * @param srcSize The number of pixels per line for the given area. Height of the full input.
	  * @param dstSize The size of the full output.
	  */
	virtual void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) = 0;

	virtual void uploadBlock(
		unsigned srcStartY, unsigned srcEndY,
		unsigned lineWidth, FrameSource& paintFrame);

protected:
	explicit GLScaler(const std::string& progName);

	/** Helper method to draw a rectangle with multiple texture coordinates.
	  * This method is similar to Texture::drawRect() but it calculates a
	  * seconds set of texture coordinates. The first tex-coords are used
	  * for the MSX texture (texture unit 0), and are calculated from
	  * src{Start,End}Y and src.getHeight(). The second set of tex-coord are
	  * used for the superimpose texture (texture unit 1) and are calculated
	  * from src{Start,End}Y and logSrcHeight.
	  * @param src
	  * @param superImpose
	  * @param srcStartY
	  * @param srcEndY
	  * @param srcSize
	  * @param dstSize
	  * @param textureFromZero If true, the texture coordinates of subpixels
	  *   will start from zero: for example in 4x zoom the source coordinates
	  *   will be 0.0, 0.25, 0.5, 0.75. If false, the texture coordinates of
	  *   subpixels will be centered: for example in 4x zoom the source
	  *   coordinates will be 0.125, 0.375, 0.625, 0.875.
	  */
	void execute(const gl::ColorTexture& src, const gl::ColorTexture* superImpose,
	             unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize,
	             bool textureFromZero = false);

protected:
	std::array<gl::BufferObject, 2> vbo;
	std::array<gl::ShaderProgram, 2> program;
	std::array<GLint, 2> unifTexSize;
	std::array<GLint, 2> unifMvpMatrix;
};

} // namespace openmsx

#endif // GLSCALER_HH
