// $Id$

#include "GLPostProcessor.hh"
#include "GLScaler.hh"
#include "GLScalerFactory.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "EnumSetting.hh"
#include "OutputSurface.hh"
#include "RawFrame.hh"
#include "Math.hh"
#include "MemoryOps.hh"
#include "InitException.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

GLPostProcessor::GLPostProcessor(
	MSXMotherBoard& motherBoard, Display& display,
	OutputSurface& screen, VideoSource videoSource,
	unsigned maxWidth, unsigned height_)
	: PostProcessor(motherBoard, display, screen,
	                videoSource, maxWidth, height_)
	, noiseTextureA(256, 256)
	, noiseTextureB(256, 256)
	, height(height_)
{
	if (!GLEW_VERSION_2_0) {
		throw InitException(
			"Your video card (or less likely video card driver) "
			"doesn't support OpenGL 2.0. It's required for the "
			"SDLGL-PP renderer.");
	}
	if (!glewIsSupported("GL_EXT_framebuffer_object")) {
		throw InitException(
			"The OpenGL framebuffer object is not supported by "
			"this glew library. Please upgrade your glew library.\n"
			"It's also possible (but less likely) your video card "
			"or video card driver doesn't support framebuffer "
			"objects.");
	}

	scaleAlgorithm = static_cast<RenderSettings::ScaleAlgorithm>(-1); // not a valid scaler

	frameCounter = 0;
	noiseX = 0.0;
	noiseY = 0.0;
	preCalcNoise(renderSettings.getNoise().getValue());

	storedFrame = false;
	for (int i = 0; i < 2; ++i) {
		colorTex[i].reset(new Texture());
		colorTex[i]->bind();
		colorTex[i]->setWrapMode(false);
		colorTex[i]->enableInterpolation();
		glTexImage2D(GL_TEXTURE_2D,     // target
			     0,                 // level
			     GL_RGB8,           // internal format
			     screen.getWidth(), // width
			     screen.getHeight(),// height
			     0,                 // border
			     GL_RGB,            // format
			     GL_UNSIGNED_BYTE,  // type
			     nullptr);          // data
		fbo[i].reset(new FrameBufferObject(*colorTex[i]));
	}

	monitor3DList = glGenLists(1);
	preCalc3DDisplayList(renderSettings.getHorizontalStretch().getValue());

	renderSettings.getNoise().attach(*this);
	renderSettings.getHorizontalStretch().attach(*this);
}

GLPostProcessor::~GLPostProcessor()
{
	renderSettings.getHorizontalStretch().detach(*this);
	renderSettings.getNoise().detach(*this);

	glDeleteLists(monitor3DList, 1);

	for (Textures::iterator it = textures.begin();
	     it != textures.end(); ++it) {
		delete it->second.tex;
		delete it->second.pbo;
	}
}

void GLPostProcessor::createRegions()
{
	regions.clear();

	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = screen.getHeight();

	unsigned g = Math::gcd(srcHeight, dstHeight);
	unsigned srcStep = srcHeight / g;
	unsigned dstStep = dstHeight / g;

	// TODO: Store all MSX lines in RawFrame and only scale the ones that fit
	//       on the PC screen, as a preparation for resizable output window.
	unsigned srcStartY = 0;
	unsigned dstStartY = 0;
	while (dstStartY < dstHeight) {
		// Currently this is true because the source frame height
		// is always >= dstHeight/(dstStep/srcStep).
		assert(srcStartY < srcHeight);

		// get region with equal lineWidth
		unsigned lineWidth = getLineWidth(paintFrame, srcStartY, srcStep);
		unsigned srcEndY = srcStartY + srcStep;
		unsigned dstEndY = dstStartY + dstStep;
		while ((srcEndY < srcHeight) && (dstEndY < dstHeight) &&
		       (getLineWidth(paintFrame, srcEndY, srcStep) == lineWidth)) {
			srcEndY += srcStep;
			dstEndY += dstStep;
		}

		regions.push_back(Region(srcStartY, srcEndY,
		                         dstStartY, dstEndY,
		                         lineWidth));

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}
}


void GLPostProcessor::paint(OutputSurface& /*output*/)
{
	RenderSettings::DisplayDeform deform =
		renderSettings.getDisplayDeform().getValue();
	double horStretch = renderSettings.getHorizontalStretch().getValue();
	int glow = renderSettings.getGlow().getValue();
	bool renderToTexture = (deform != RenderSettings::DEFORM_NORMAL) ||
	                       (horStretch != 320.0) ||
	                       (glow != 0);

	if ((deform == RenderSettings::DEFORM_3D) || !paintFrame) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (!paintFrame) {
			return;
		}
	}

	// New scaler algorithm selected?
	RenderSettings::ScaleAlgorithm algo =
		renderSettings.getScaleAlgorithm().getValue();
	if (scaleAlgorithm != algo) {
		scaleAlgorithm = algo;
		currScaler = GLScalerFactory::createScaler(renderSettings);

		// Re-upload frame data, this is both
		//  - Chunks of RawFrame with a specific linewidth, possibly
		//    with some extra lines above and below each chunk that are
		//    also converted to this linewidth.
		//  - Extra data that is specific for the scaler (ATM only the
		//    hq and hqlite scalers require this).
		// Re-uploading the first is not strictly needed. But switching
		// scalers doesn't happen that often, so it also doesn't hurt
		// and it keeps the code simpler.
		uploadFrame();
	}

	if (renderToTexture) {
		glViewport(0, 0, screen.getWidth(), screen.getHeight());
		glBindTexture(GL_TEXTURE_2D, 0);
		fbo[frameCounter & 1]->push();
	}

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (Regions::const_iterator it = regions.begin();
	     it != regions.end(); ++it) {
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	it->srcStartY, it->srcEndY, it->lineWidth);
		assert(textures.find(it->lineWidth) != textures.end());
		ColorTexture* superImpose = superImposeVideoFrame
		                          ? superImposeTex.get() : nullptr;
		currScaler->scaleImage(
			*textures[it->lineWidth].tex, superImpose,
			it->srcStartY, it->srcEndY, it->lineWidth,     // src
			it->dstStartY, it->dstEndY, screen.getWidth(), // dst
			paintFrame->getHeight()); // dst
		//GLUtil::checkGLError("GLPostProcessor::paint");
	}

	ShaderProgram::deactivate();

	drawNoise();
	drawGlow(glow);

	if (renderToTexture) {
		fbo[frameCounter & 1]->pop();
		colorTex[frameCounter & 1]->bind();
		glViewport(screen.getX(), screen.getY(),
		           screen.getWidth(), screen.getHeight());

		glEnable(GL_TEXTURE_2D);
		if (deform == RenderSettings::DEFORM_3D) {
			glCallList(monitor3DList);
		} else {
			glBegin(GL_QUADS);
			int w = screen.getWidth();
			int h = screen.getHeight();
			GLfloat x1 = (320.0f - GLfloat(horStretch)) / (2.0f * 320.0f);
			GLfloat x2 = 1.0f - x1;
			glTexCoord2f(x1, 0.0f); glVertex2i(0, h);
			glTexCoord2f(x1, 1.0f); glVertex2i(0, 0);
			glTexCoord2f(x2, 1.0f); glVertex2i(w, 0);
			glTexCoord2f(x2, 0.0f); glVertex2i(w, h);
			glEnd();
		}
		glDisable(GL_TEXTURE_2D);
		storedFrame = true;
	} else {
		storedFrame = false;
	}
}

std::unique_ptr<RawFrame> GLPostProcessor::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, FrameSource::FieldType field,
	EmuTime::param time)
{
	std::unique_ptr<RawFrame> reuseFrame =
		PostProcessor::rotateFrames(std::move(finishedFrame), field, time);
	uploadFrame();
	++frameCounter;
	noiseX = double(rand()) / RAND_MAX;
	noiseY = double(rand()) / RAND_MAX;
	return reuseFrame;
}

void GLPostProcessor::update(const Setting& setting)
{
	VideoLayer::update(setting);
	FloatSetting& noiseSetting = renderSettings.getNoise();
	FloatSetting& horizontalStretch = renderSettings.getHorizontalStretch();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getValue());
	} else if (&setting == &horizontalStretch) {
		preCalc3DDisplayList(horizontalStretch.getValue());
	}
}

void GLPostProcessor::uploadFrame()
{
	createRegions();

	const unsigned srcHeight = paintFrame->getHeight();
	for (Regions::const_iterator it = regions.begin();
	     it != regions.end(); ++it) {
		// upload data
		// TODO get before/after data from scaler
		unsigned before = 1;
		unsigned after  = 1;
		uploadBlock(std::max<int>(0,         it->srcStartY - before),
		            std::min<int>(srcHeight, it->srcEndY   + after),
		            it->lineWidth);
	}

	if (superImposeVideoFrame) {
		int width  = superImposeVideoFrame->getWidth();
		int height = superImposeVideoFrame->getHeight();
		if (!superImposeTex.get() ||
		    superImposeTex->getWidth()  != width ||
		    superImposeTex->getHeight() != height) {
			superImposeTex.reset(new ColorTexture(width, height));
			superImposeTex->enableInterpolation();
		}
		superImposeTex->bind();
		glTexSubImage2D(
			GL_TEXTURE_2D,     // target
			0,                 // level
			0,                 // offset x
			0,                 // offset y
			width,             // width
			height,            // height
			GL_BGRA,           // format
			GL_UNSIGNED_BYTE,  // type
			const_cast<RawFrame*>(superImposeVideoFrame)->getLinePtrDirect<unsigned>(0)); // data
	}
}

void GLPostProcessor::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth)
{
	// create texture/pbo if needed
	Textures::iterator it = textures.find(lineWidth);
	ColorTexture* tex;
	PixelBuffer<unsigned>* pbo;
	if (it != textures.end()) {
		tex = it->second.tex;
		pbo = it->second.pbo;
	} else {
		tex = new ColorTexture(lineWidth, height * 2); // *2 for interlace
		tex->setWrapMode(false);

		pbo = new PixelBuffer<unsigned>();
		if (pbo->openGLSupported()) {
			pbo->setImage(lineWidth, height * 2);
		} else {
			delete pbo;
			pbo = 0;
		}

		TextureData textureData;
		textureData.tex = tex;
		textureData.pbo = pbo;
		textures[lineWidth] = textureData;
	}

	// bind texture
	tex->bind();

	// upload data
	unsigned* mapped;
	if (pbo) {
		pbo->bind();
		mapped = pbo->mapWrite();
	} else {
		mapped = 0;
	}
	if (mapped) {
		for (unsigned y = srcStartY; y < srcEndY; ++y) {
			const unsigned* data =
				paintFrame->getLinePtr<unsigned>(y, lineWidth);
			MemoryOps::stream_memcpy(
				mapped + y * lineWidth, data, lineWidth);
			paintFrame->freeLineBuffers(); // ASAP to keep cache warm
		}
		pbo->unmap();
#if defined(__APPLE__)
		// The nVidia GL driver for the GeForce 8000/9000 series seems to hang
		// on texture data replacements that are 1 pixel wide and start on a
		// line number that is a non-zero multiple of 16.
		if (lineWidth == 1 && srcStartY != 0 && srcStartY % 16 == 0) {
			srcStartY--;
		}
#endif
		glTexSubImage2D(
			GL_TEXTURE_2D,       // target
			0,                   // level
			0,                   // offset x
			srcStartY,           // offset y
			lineWidth,           // width
			srcEndY - srcStartY, // height
			GL_BGRA,             // format
			GL_UNSIGNED_BYTE,    // type
			pbo->getOffset(0, srcStartY)); // data
	}
	if (pbo) {
		pbo->unbind();
	}
	if (!mapped) {
		glPixelStorei(GL_UNPACK_ROW_LENGTH, paintFrame->getRowLength());
		unsigned y = srcStartY;
		unsigned remainingLines = srcEndY - srcStartY;
		while (remainingLines) {
			unsigned lines;
			const unsigned* data = paintFrame->getMultiLinePtr<unsigned>(
				y, remainingLines, lines, lineWidth);
			glTexSubImage2D(
				GL_TEXTURE_2D,     // target
				0,                 // level
				0,                 // offset x
				y,                 // offset y
				lineWidth,         // width
				lines,             // height
				GL_BGRA,           // format
				GL_UNSIGNED_BYTE,  // type
				data);             // data
			paintFrame->freeLineBuffers(); // ASAP to keep cache warm

			y += lines;
			remainingLines -= lines;
		}
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // restore default
	}

	// possibly upload scaler specific data
	if (currScaler.get()) {
		currScaler->uploadBlock(srcStartY, srcEndY, lineWidth, *paintFrame);
	}
}

void GLPostProcessor::drawGlow(int glow)
{
	if ((glow == 0) || !storedFrame) return;

	colorTex[(frameCounter & 1) ^ 1]->bind();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	GLfloat alpha = glow * 31 / 3200.0f;
	glColor4f(0.0f, 0.0f, 0.0f, alpha);
	int w = screen.getWidth();
	int h = screen.getHeight();
	glTexCoord2i(0, 0); glVertex2i(0, h);
	glTexCoord2i(0, 1); glVertex2i(0, 0);
	glTexCoord2i(1, 1); glVertex2i(w, 0);
	glTexCoord2i(1, 0); glVertex2i(w, h);
	glEnd();
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

void GLPostProcessor::preCalcNoise(double factor)
{
	GLbyte buf1[256 * 256];
	GLbyte buf2[256 * 256];
	for (int i = 0; i < 256 * 256; i += 2) {
		double r1, r2;
		Math::gaussian2(r1, r2);
		int s1 = Math::clip<-255, 255>(r1, factor);
		buf1[i + 0] = (s1 > 0) ?  s1 : 0;
		buf2[i + 0] = (s1 < 0) ? -s1 : 0;
		int s2 = Math::clip<-255, 255>(r2, factor);
		buf1[i + 1] = (s2 > 0) ?  s2 : 0;
		buf2[i + 1] = (s2 < 0) ? -s2 : 0;
	}
	noiseTextureA.updateImage(0, 0, 256, 256, buf1);
	noiseTextureB.updateImage(0, 0, 256, 256, buf2);
}

void GLPostProcessor::drawNoise()
{
	if (renderSettings.getNoise().getValue() == 0) return;

	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static const int coord[8][4][2] = {
		{ {   0,   0 }, { 320,   0 }, { 320, 240 }, {   0, 240 } },
		{ {   0, 240 }, { 320, 240 }, { 320,   0 }, {   0,   0 } },
		{ {   0, 240 }, {   0,   0 }, { 320,   0 }, { 320, 240 } },
		{ { 320, 240 }, { 320,   0 }, {   0,   0 }, {   0, 240 } },
		{ { 320, 240 }, {   0, 240 }, {   0,   0 }, { 320,   0 } },
		{ { 320,   0 }, {   0,   0 }, {   0, 240 }, { 320, 240 } },
		{ { 320,   0 }, { 320, 240 }, {   0, 240 }, {   0,   0 } },
		{ {   0,   0 }, {   0, 240 }, { 320, 240 }, { 320,   0 } }
	};
	int zoom = renderSettings.getScaleFactor().getValue();

	unsigned seq = frameCounter & 7;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	noiseTextureA.bind();
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + GLfloat(noiseX), 1.875f + GLfloat(noiseY));
	glVertex2i(coord[seq][0][0] * zoom, coord[seq][0][1] * zoom);
	glTexCoord2f(2.5f + GLfloat(noiseX), 1.875f + GLfloat(noiseY));
	glVertex2i(coord[seq][1][0] * zoom, coord[seq][1][1] * zoom);
	glTexCoord2f(2.5f + GLfloat(noiseX), 0.000f + GLfloat(noiseY));
	glVertex2i(coord[seq][2][0] * zoom, coord[seq][2][1] * zoom);
	glTexCoord2f(0.0f + GLfloat(noiseX), 0.000f + GLfloat(noiseY));
	glVertex2i(coord[seq][3][0] * zoom, coord[seq][3][1] * zoom);
	glEnd();
	// Note: If glBlendEquation is not present, the second noise texture will
	//       be added instead of subtracted, which means there will be no noise
	//       on white pixels. A pity, but it's better than no noise at all.
	if (glBlendEquation) glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	noiseTextureB.bind();
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + GLfloat(noiseX), 1.875f + GLfloat(noiseY));
	glVertex2i(coord[seq][0][0] * zoom, coord[seq][0][1] * zoom);
	glTexCoord2f(2.5f + GLfloat(noiseX), 1.875f + GLfloat(noiseY));
	glVertex2i(coord[seq][1][0] * zoom, coord[seq][1][1] * zoom);
	glTexCoord2f(2.5f + GLfloat(noiseX), 0.000f + GLfloat(noiseY));
	glVertex2i(coord[seq][2][0] * zoom, coord[seq][2][1] * zoom);
	glTexCoord2f(0.0f + GLfloat(noiseX), 0.000f + GLfloat(noiseY));
	glVertex2i(coord[seq][3][0] * zoom, coord[seq][3][1] * zoom);
	glEnd();
	glPopAttrib();
	if (glBlendEquation) glBlendEquation(GL_FUNC_ADD);
}

void GLPostProcessor::preCalc3DDisplayList(double width)
{
	// generate display list for 3d deform
	static const int GRID_SIZE = 16;
	struct Point {
		GLfloat vx, vy, vz;
		GLfloat nx, ny, nz;
		GLfloat tx, ty;
	} points[GRID_SIZE + 1][GRID_SIZE + 1];
	const int GRID_SIZE2 = GRID_SIZE / 2;
	GLfloat s = GLfloat(width) / 320.0f;
	GLfloat b = (320.0f - GLfloat(width)) / (2.0f * 320.0f);

	for (int sx = 0; sx <= GRID_SIZE; ++sx) {
		for (int sy = 0; sy <= GRID_SIZE; ++sy) {
			Point& p = points[sx][sy];
			GLfloat x = GLfloat(sx - GRID_SIZE2) / GRID_SIZE2;
			GLfloat y = GLfloat(sy - GRID_SIZE2) / GRID_SIZE2;

			p.vx = x;
			p.vy = y;
			p.vz = (x * x + y * y) / -12.0f;

			p.nx = x / 6.0f;
			p.ny = y / 6.0f;
			p.nz = 1.0f;      // note: not normalized

			p.tx = (GLfloat(sx) / GRID_SIZE) * s + b;
			p.ty = GLfloat(sy) / GRID_SIZE;
		}
	}

	GLfloat LightDiffuse[]= { 1.2f, 1.2f, 1.2f, 1.2f };
	glLightfv(GL_LIGHT0, GL_DIFFUSE, LightDiffuse);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);

	glNewList(monitor3DList, GL_COMPILE);
	glEnable(GL_LIGHTING);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glFrustum(-1, 1, -1, 1, 1, 10);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(0.0f, 0.4f, -2.0f);
	glRotatef(-10.0f, 1.0f, 0.0f, 0.0f);
	glScalef(2.2f, 2.2f, 2.2f);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	for (int y = 0; y < GRID_SIZE; ++y) {
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < (GRID_SIZE + 1); ++x) {
			Point& p1 = points[x][y + 0];
			Point& p2 = points[x][y + 1];
			glTexCoord2f(p1.tx, p1.ty);
			glNormal3f  (p1.nx, p1.ny, p1.nz);
			glVertex3f  (p1.vx, p1.vy, p1.vz);
			glTexCoord2f(p2.tx, p2.ty);
			glNormal3f  (p2.nx, p2.ny, p2.nz);
			glVertex3f  (p2.vx, p2.vy, p2.vz);
		}
		glEnd();
	}
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glDisable(GL_LIGHTING);
	glEndList();
}

} // namespace openmsx
