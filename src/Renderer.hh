// $Id$

#ifndef __RENDERER_HH__
#define __RENDERER_HH__

class Renderer
{
public:

	virtual void toggleFullScreen() = NULL;

	/** Put an image on the screen.
	  */
	virtual void putImage() = NULL;

	virtual void fullScreenRefresh() = NULL;

};

#endif //__RENDERER_HH__

