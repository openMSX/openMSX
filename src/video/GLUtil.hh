// $Id$

#ifndef __GLUTIL_HH__
#define __GLUTIL_HH__

// Check for availability of OpenGL headers.
#include "config.h"
#if (defined(HAVE_GL_GL_H) || defined(HAVE_GL_H))
#define __OPENGL_AVAILABLE__

// Include OpenGL headers.
#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#else // HAVE_GL_H
#include <gl.h>
#endif

#endif // OpenGL header check.
#endif // __GLUTIL_HH__
