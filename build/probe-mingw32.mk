# $Id$

GL_LDFLAGS:=-lopengl32

GLEW_CFLAGS_3RD_STA+=-DGLEW_STATIC
GLEW_LDFLAGS_SYS_DYN:=-lglew32
GLEW_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libglew32.a $(GL_LDFLAGS)

SDL_LDFLAGS_SYS_DYN:=`sdl-config --libs | sed -e 's/-mwindows/-mconsole/'`
SDL_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libSDL.a /mingw/lib/libmingw32.a $(3RDPARTY_INSTALL_DIR)/lib/libSDLmain.a -lwinmm -lgdi32
