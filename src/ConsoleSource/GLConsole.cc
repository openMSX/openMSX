// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include "SDL/SDL_image.h"
#include "GLConsole.hh"
#include "CommandController.hh"
#include "HotKey.hh"
#include "ConsoleManager.hh"
#include "FileOpener.hh"
#include "GLFont.hh"
#include "EventDistributor.hh"
#include "MSXConfig.hh"


GLConsole::GLConsole() :
	consoleCmd(this)
{
	SDL_Surface *screen = SDL_GetVideoSurface();
	isVisible = false;
	blink = false;
	lastBlinkTime = 0;
	backgroundTexture = 0;
	dispX         = (screen->w / 32);
	dispY         = (screen->h / 15) * 9;
	consoleWidth  = (screen->w / 32) * 30;
	consoleHeight = (screen->h / 15) * 6;
	
	SDL_EnableUNICODE(1);
	
	int width, height;
	GLfloat fontTexCoord[4];
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("Console");
	std::string fontName = config->getParameter("font");
	GLuint fontTexture = loadTexture(FileOpener::findFileName(fontName),
	                                 width, height, fontTexCoord);
	font = new GLFont(fontTexture, width, height, fontTexCoord);

	try {
		int width, height;
		std::string backgroundName = config->getParameter("background");
		backgroundTexture = loadTexture(FileOpener::findFileName(backgroundName),
		                                width, height, backTexCoord);
	} catch(MSXException &e) {
		// no background or missing file
	}

	ConsoleManager::instance()->registerConsole(this);
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP,   this);
	CommandController::instance()->registerCommand(consoleCmd, "console");
	HotKey::instance()->registerHotKeyCommand(Keys::K_F10, "console");
}

GLConsole::~GLConsole()
{
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_F10, "console");
	CommandController::instance()->unregisterCommand("console");
	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP,   this);
	ConsoleManager::instance()->unregisterConsole(this);
	delete font;
	glDeleteTextures(1, &backgroundTexture);
}


int GLConsole::powerOfTwo(int a)
{
	int res = 1;
	while (a > res)
		res <<= 1;
	return res;
}

GLuint GLConsole::loadTexture(const std::string &filename, int &width, int &height, GLfloat *texCoord)
{
	SDL_Surface* image1 = IMG_Load(filename.c_str());
	if (image1 == NULL)
		throw MSXException("Error loading texture");
	SDL_SetAlpha(image1, 0, 0);
		
	width  = image1->w;
	height = image1->h;
	int w2 = powerOfTwo(width);
	int h2 = powerOfTwo(height);
	texCoord[0] = 0.0f;			// Min X
	texCoord[1] = 0.0f;			// Min Y
	texCoord[2] = (GLfloat)width  / w2;	// Max X
	texCoord[3] = (GLfloat)height / h2;	// Max Y

	SDL_Surface* image2 = SDL_CreateRGBSurface(SDL_SWSURFACE, w2, h2, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
		0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
	);
	if (image2 == NULL)
		throw MSXException("Error loading texture");
	
	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_BlitSurface(image1, &area, image2, &area);
	SDL_FreeSurface(image1);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	SDL_FreeSurface(image2);

	return texture;
}


// Takes keys from the keyboard and inputs them to the console
bool GLConsole::signalEvent(SDL_Event &event)
{
	if (!isVisible)
		return true;
	if (event.type == SDL_KEYUP)
		return false;	// don't pass event to MSX-Keyboard
	
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	switch (key) {
		case Keys::K_PAGEUP:
			scrollUp();
			break;
		case Keys::K_PAGEDOWN:
			scrollDown();
			break;
		case Keys::K_UP:
			prevCommand();
			break;
		case Keys::K_DOWN:
			nextCommand();
			break;
		case Keys::K_BACKSPACE:
			backspace();
			break;
		case Keys::K_TAB:
			tabCompletion();
			break;
		case Keys::K_RETURN:
			commandExecute();
			break;
		default:
			if (event.key.keysym.unicode)
				normalKey((char)event.key.keysym.unicode);
	}
	return false;	// don't pass event to MSX-Keyboard
}

// Updates the console buffer
void GLConsole::updateConsole()
{
}

// Draws the console buffer to the screen
void GLConsole::drawConsole()
{
	if (!isVisible) return;

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	SDL_Surface *screen = SDL_GetVideoSurface();
	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0, screen->w, screen->h, 0.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTranslated(dispX, dispY, 0);
	// draw the background image if there is one
	if (backgroundTexture) {
		glBindTexture(GL_TEXTURE_2D, backgroundTexture);
		glBegin(GL_QUADS);
		glTexCoord2f(backTexCoord[0], backTexCoord[1]);
		glVertex2i(0, 0);
		glTexCoord2f(backTexCoord[0], backTexCoord[3]);
		glVertex2i(0, consoleHeight);
		glTexCoord2f(backTexCoord[2], backTexCoord[3]);
		glVertex2i(consoleWidth, consoleHeight);
		glTexCoord2f(backTexCoord[2], backTexCoord[1]);
		glVertex2i(consoleWidth, 0);
		glEnd();
	}

	int screenlines = consoleHeight / font->height();
	for (int loop=0; loop<screenlines; loop++) {
		int num = loop+consoleScrollBack;
		if (num < lines.size()) {
			glPushMatrix();
			font->drawText(lines[num], CHAR_BORDER,
			               consoleHeight - (1+loop)*font->height());
			glPopMatrix();
		}
	}
	
	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
	}
	if (consoleScrollBack == 0) {
		if (blink) {
			// Print cursor if there is enough room
			int cursorLocation = lines[0].length();
			font->drawText(std::string("_"), 
				      CHAR_BORDER + cursorLocation * font->width(),
				      consoleHeight - font->height());
			
		}
	}
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}



// Console command
GLConsole::ConsoleCmd::ConsoleCmd(GLConsole *cons)
{
	console = cons;
}

void GLConsole::ConsoleCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
	case 1:
		console->isVisible = !console->isVisible;
		break;
	case 2:
		if (tokens[1] == "on") {
			console->isVisible = true;
			break;
		} 
		if (tokens[1] == "off") {
			console->isVisible = false;
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}
void GLConsole::ConsoleCmd::help   (const std::vector<std::string> &tokens)
{
	print("This command turns console display on/off");
	print(" console:     toggle console display");
	print(" console on:  show console display");
	print(" console off: remove console display");
} 



