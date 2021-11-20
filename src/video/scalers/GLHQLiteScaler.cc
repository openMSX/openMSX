#include "GLHQLiteScaler.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "vla.hh"
#include <cstring>
#include <utility>

namespace openmsx {

GLHQLiteScaler::GLHQLiteScaler(GLScaler& fallback_)
	: GLScaler("hqlite")
	, fallback(fallback_)
{
	for (auto& p : program) {
		p.activate();
		glUniform1i(p.getUniformLocation("edgeTex"),   2);
		glUniform1i(p.getUniformLocation("offsetTex"), 3);
	}

	// GL_LUMINANCE_ALPHA is no longer supported in newer openGL versions
	auto format = (OPENGL_VERSION >= OPENGL_3_3) ? GL_RG : GL_LUMINANCE_ALPHA;
	edgeTexture.bind();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             format,           // internal format
	             320,              // width
	             240,              // height
	             0,                // border
	             format,           // format
	             GL_UNSIGNED_BYTE, // type
	             nullptr);         // data
#if OPENGL_VERSION >= OPENGL_3_3
	GLint swizzleMask1[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask1);
#endif
	edgeBuffer.setImage(320, 240);

	auto context = systemFileContext();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	std::string offsetName = "shaders/HQ_xLiteOffsets.dat";
	for (auto i : xrange(3)) {
		int n = i + 2;
		offsetName[10] = char('0') + n;
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
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		setup(superImpose != nullptr);
		src.setInterpolation(true);
		glActiveTexture(GL_TEXTURE3);
		offsetTexture[factorX - 2].bind();
		glActiveTexture(GL_TEXTURE2);
		edgeTexture.bind();
		glActiveTexture(GL_TEXTURE0);
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
	if ((lineWidth != 320) || (srcEndY > 240)) return;

	Endian::L32 tmpBuf2[320 / 2]; // 2 x uint16_t
	#ifndef NDEBUG
	// Avoid UMR. In optimized mode we don't care.
	memset(tmpBuf2, 0, sizeof(tmpBuf2));
	#endif

	VLA_SSE_ALIGNED(Pixel, buf1_, lineWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, lineWidth); auto* buf2 = buf2_;
	const auto* curr = paintFrame.getLinePtr(srcStartY - 1, lineWidth, buf1);
	const auto* next = paintFrame.getLinePtr(srcStartY + 0, lineWidth, buf2);
	calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());

	edgeBuffer.bind();
	if (auto* mapped = edgeBuffer.mapWrite()) {
		for (auto y : xrange(srcStartY, srcEndY)) {
			curr = next;
			std::swap(buf1, buf2);
			next = paintFrame.getLinePtr(y + 1, lineWidth, buf2);
			calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());
			memcpy(mapped + 320 * y, tmpBuf2, 320 * sizeof(uint16_t));
		}
		edgeBuffer.unmap();

		auto format = (OPENGL_VERSION >= OPENGL_3_3) ? GL_RG : GL_LUMINANCE_ALPHA;
		edgeTexture.bind();
		glTexSubImage2D(GL_TEXTURE_2D,       // target
		                0,                   // level
		                0,                   // offset x
		                srcStartY,           // offset y
		                lineWidth,           // width
		                srcEndY - srcStartY, // height
		                format,              // format
		                GL_UNSIGNED_BYTE,    // type
		                edgeBuffer.getOffset(0, srcStartY)); // data
	}
	edgeBuffer.unbind();
}

} // namespace openmsx
