#include "GLHQScaler.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "vla.hh"
#include <cstring>
#include <utility>

using std::string;

namespace openmsx {

GLHQScaler::GLHQScaler(GLScaler& fallback_)
	: GLScaler("hq")
	, fallback(fallback_)
{
	for (auto& p : program) {
		p.activate();
		glUniform1i(p.getUniformLocation("edgeTex"),   2);
		glUniform1i(p.getUniformLocation("offsetTex"), 3);
		glUniform1i(p.getUniformLocation("weightTex"), 4);
	}

	edgeTexture.bind();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_R16,           // internal format
	             320,              // width
	             240,              // height
	             0,                // border
	             GL_RED,           // format
	             GL_UNSIGNED_SHORT,// type
	             nullptr);         // data
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	edgeBuffer.setImage(320, 240);

	auto context = systemFileContext();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	string offsetsName = "shaders/HQ_xOffsets.dat";
	string weightsName = "shaders/HQ_xWeights.dat";
	for (int i = 0; i < 3; ++i) {
		int n = i + 2;
		offsetsName[10] = char('0') + n;
		File offsetsFile(context.resolve(offsetsName));
		offsetTexture[i].bind();
		glTexImage2D(GL_TEXTURE_2D,       // target
		             0,                   // level
		             GL_RGBA8,            // internal format
		             n * 64,              // width
		             n * 64,              // height
		             0,                   // border
		             GL_RGBA,             // format
		             GL_UNSIGNED_BYTE,    // type
		             offsetsFile.mmap().data());// data

		weightsName[10] = char('0') + n;
		File weightsFile(context.resolve(weightsName));
		weightTexture[i].bind();
		glTexImage2D(GL_TEXTURE_2D,       // target
		             0,                   // level
		             GL_RGB8,             // internal format
		             n * 64,              // width
		             n * 64,              // height
		             0,                   // border
		             GL_RGB,              // format
		             GL_UNSIGNED_BYTE,    // type
		             weightsFile.mmap().data());// data
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore to default
}

void GLHQScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		assert(src.getHeight() == 2 * 240);
		setup(superImpose != nullptr);
		glActiveTexture(GL_TEXTURE4);
		weightTexture[factorX - 2].bind();
		glActiveTexture(GL_TEXTURE3);
		offsetTexture[factorX - 2].bind();
		glActiveTexture(GL_TEXTURE2);
		edgeTexture.bind();
		glActiveTexture(GL_TEXTURE0);
		execute(src, superImpose,
		        srcStartY, srcEndY, srcWidth,
		        dstStartY, dstEndY, dstWidth,
		        logSrcHeight);
	} else {
		fallback.scaleImage(src, superImpose,
		                    srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth,
		                    logSrcHeight);
	}
}

using Pixel = uint32_t;
void GLHQScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if ((lineWidth != 320) || (srcEndY > 240)) return;

	uint32_t tmpBuf2[320 / 2]; // 2 x uint16_t
	#ifndef NDEBUG
	// Avoid UMR. In optimized mode we don't care.
	memset(tmpBuf2, 0, sizeof(tmpBuf2));
	#endif

	VLA_SSE_ALIGNED(Pixel, buf1_, lineWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, lineWidth); auto* buf2 = buf2_;
	auto* curr = paintFrame.getLinePtr(srcStartY - 1, lineWidth, buf1);
	auto* next = paintFrame.getLinePtr(srcStartY + 0, lineWidth, buf2);
	EdgeHQ edgeOp(0, 8, 16);
	calcEdgesGL(curr, next, tmpBuf2, edgeOp);

	edgeBuffer.bind();
	if (auto* mapped = edgeBuffer.mapWrite()) {
		for (unsigned y = srcStartY; y < srcEndY; ++y) {
			curr = next;
			std::swap(buf1, buf2);
			next = paintFrame.getLinePtr(y + 1, lineWidth, buf2);
			calcEdgesGL(curr, next, tmpBuf2, edgeOp);
			memcpy(mapped + 320 * y, tmpBuf2, 320 * sizeof(uint16_t));
		}
		edgeBuffer.unmap();

		edgeTexture.bind();
		glTexSubImage2D(GL_TEXTURE_2D,       // target
		                0,                   // level
		                0,                   // offset x
		                srcStartY,           // offset y
		                lineWidth,           // width
		                srcEndY - srcStartY, // height
		                GL_RED,              // format
		                GL_UNSIGNED_SHORT,   // type
		                edgeBuffer.getOffset(0, srcStartY)); // data
	}
	edgeBuffer.unbind();
}

} // namespace openmsx
