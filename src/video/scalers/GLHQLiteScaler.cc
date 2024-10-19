#include "GLHQLiteScaler.hh"

#include "File.hh"
#include "FileContext.hh"
#include "FrameSource.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "vla.hh"

#include <array>
#include <cstring>
#include <utility>

namespace openmsx {

GLHQLiteScaler::GLHQLiteScaler(GLScaler& fallback_, unsigned maxWidth_, unsigned maxHeight_)
	: GLScaler("hqlite")
	, fallback(fallback_)
	, maxWidth(maxWidth_), maxHeight(maxHeight_)
{
	for (auto i : xrange(2)) {
		auto& p = program[i];
		p.activate();
		glUniform1i(p.getUniformLocation("edgeTex"),   2);
		glUniform1i(p.getUniformLocation("offsetTex"), 3);
		edgePosScaleUnif[i] = p.getUniformLocation("edgePosScale");
	}

	// GL_LUMINANCE_ALPHA is no longer supported in newer openGL versions
	auto format = (OPENGL_VERSION >= OPENGL_3_3) ? GL_RG : GL_LUMINANCE_ALPHA;
	edgeTexture.bind();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             format,           // internal format
	             maxWidth,         // width
	             maxHeight,        // height
	             0,                // border
	             format,           // format
	             GL_UNSIGNED_BYTE, // type
	             nullptr);         // data
#if OPENGL_VERSION >= OPENGL_3_3
	GLint swizzleMask1[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask1);
#endif
	edgeBuffer.allocate(maxWidth * maxHeight);

	const auto& context = systemFileContext();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	std::string offsetName = "shaders/HQ_xLiteOffsets.dat";
	for (auto i : xrange(3)) {
		int n = i + 2;
		offsetName[10] = narrow<char>('0' + n);
		File offsetFile(context.resolve(offsetName));
		offsetTexture[i].bind();
		glTexImage2D(GL_TEXTURE_2D,        // target
		             0,                    // level
		             format,               // internal format
		             n * 64,               // width
		             n * 64,               // height
		             0,                    // border
		             format,               // format
		             GL_UNSIGNED_BYTE,     // type
		             offsetFile.mmap().data());// data
#if OPENGL_VERSION >= OPENGL_3_3
		GLint swizzleMask2[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask2);
#endif
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore to default
}

void GLHQLiteScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY); // 1 - 4

	if ((factorY >= 2) && ((srcWidth % 320) == 0)) {
		setup(superImpose != nullptr);
		src.setInterpolation(true);
		glActiveTexture(GL_TEXTURE3);
		offsetTexture[factorY - 2].bind();
		glActiveTexture(GL_TEXTURE2);
		edgeTexture.bind();
		glActiveTexture(GL_TEXTURE0);

		int i = superImpose ? 1 : 0;
		glUniform2f(edgePosScaleUnif[i], src.getWidth() / float(maxWidth), src.getHeight() / float(maxHeight));

		execute(src, superImpose,
		        srcStartY, srcEndY, srcWidth,
		        dstStartY, dstEndY, dstWidth,
		        logSrcHeight);
		src.setInterpolation(false);
	} else {
		fallback.scaleImage(src, superImpose,
		                    srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth,
		                    logSrcHeight);
	}
}

using Pixel = uint32_t;
void GLHQLiteScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if ((lineWidth % 320) != 0) return;

	assert(maxWidth <= 1280);
	std::array<Endian::L32, 1280 / 2> tmpBufMax; // 2 x uint16_t
	auto tmpBuf2 = subspan(tmpBufMax, 0, lineWidth / 2);
	#ifndef NDEBUG
	// Avoid UMR. In optimized mode we don't care.
	ranges::fill(tmpBuf2, 0);
	#endif

	VLA_SSE_ALIGNED(Pixel, buf1, lineWidth);
	VLA_SSE_ALIGNED(Pixel, buf2, lineWidth);
	auto curr = paintFrame.getLine(narrow<int>(srcStartY) - 1, buf1);
	auto next = paintFrame.getLine(narrow<int>(srcStartY) + 0, buf2);
	calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());

	edgeBuffer.bind();
	auto mapped = edgeBuffer.mapWrite();
	auto numLines = srcEndY - srcStartY;
	for (auto yy : xrange(numLines)) {
		curr = next;
		std::swap(buf1, buf2);
		next = paintFrame.getLine(narrow<int>(yy + srcStartY + 1), buf2);
		calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());
		auto dest = mapped.subspan(yy * size_t(lineWidth), lineWidth);
		assert(dest.size_bytes() == tmpBuf2.size_bytes()); // note: convert L32 -> 2 x uint16_t
		memcpy(dest.data(), tmpBuf2.data(), tmpBuf2.size_bytes());
	}
	edgeBuffer.unmap();

	auto format = (OPENGL_VERSION >= OPENGL_3_3) ? GL_RG : GL_LUMINANCE_ALPHA;
	edgeTexture.bind();
	glTexSubImage2D(GL_TEXTURE_2D,            // target
			0,                        // level
			0,                        // offset x
			narrow<GLint>(srcStartY), // offset y
			narrow<GLint>(lineWidth), // width
			narrow<GLint>(numLines),  // height
			format,                   // format
			GL_UNSIGNED_BYTE,         // type
			mapped.data());           // data
	edgeBuffer.unbind();
}

} // namespace openmsx
