# Download locations for package sources.
DOWNLOAD_ZLIB:=http://downloads.sourceforge.net/libpng
DOWNLOAD_PNG:=http://downloads.sourceforge.net/libpng
DOWNLOAD_FREETYPE:=http://downloads.sourceforge.net/freetype
DOWNLOAD_SDL:=http://www.libsdl.org/release
DOWNLOAD_SDL_IMAGE:=http://www.libsdl.org/projects/SDL_image/release
DOWNLOAD_SDL_TTF:=http://www.libsdl.org/projects/SDL_ttf/release
DOWNLOAD_GLEW:=http://downloads.sourceforge.net/glew
DOWNLOAD_TCL:=http://downloads.sourceforge.net/tcl
DOWNLOAD_XML:=http://xmlsoft.org/sources
DOWNLOAD_DIRECTX:=http://alleg.sourceforge.net/files
DOWNLOAD_OGG:=http://downloads.xiph.org/releases/ogg
DOWNLOAD_OGGZ:=http://downloads.xiph.org/releases/liboggz
DOWNLOAD_VORBIS:=http://downloads.xiph.org/releases/vorbis
DOWNLOAD_THEORA:=http://downloads.xiph.org/releases/theora

# These were the most recent versions at the moment of writing this Makefile.
# You can use other versions if you like; adjust the names accordingly.
# Note: Do not put comments behind the definition, since this will include
#       a space in the value and spaces are separators for Make so the whole
#       build process will break.
PACKAGE_ZLIB:=zlib-1.2.3
PACKAGE_PNG:=libpng-1.2.35
PACKAGE_FREETYPE:=freetype-2.3.9
PACKAGE_SDL:=SDL-1.2.13
PACKAGE_SDL_IMAGE:=SDL_image-1.2.7
PACKAGE_SDL_TTF:=SDL_ttf-2.0.9
PACKAGE_GLEW:=glew-1.5.1
PACKAGE_TCL:=tcl8.5.7
PACKAGE_XML:=libxml2-2.7.3
PACKAGE_DIRECTX:=dx70
PACKAGE_OGG:=libogg-1.1.4
PACKAGE_OGGZ:=liboggz-0.9.9
PACKAGE_VORBIS:=libvorbis-1.2.3
PACKAGE_THEORA:=libtheora-1.0

# Source tar file names for non-standard packages.
TARBALL_GLEW:=$(PACKAGE_GLEW)-src.tgz
TARBALL_TCL:=$(PACKAGE_TCL)-src.tar.gz
TARBALL_DIRECTX:=$(PACKAGE_DIRECTX)_mgw.tar.gz
# Source tar file names for standard packages.
TARBALL_ZLIB:=$(PACKAGE_ZLIB).tar.gz
TARBALL_PNG:=$(PACKAGE_PNG).tar.gz
TARBALL_FREETYPE:=$(PACKAGE_FREETYPE).tar.gz
TARBALL_SDL:=$(PACKAGE_SDL).tar.gz
TARBALL_SDL_IMAGE:=$(PACKAGE_SDL_IMAGE).tar.gz
TARBALL_SDL_TTF:=$(PACKAGE_SDL_TTF).tar.gz
TARBALL_XML:=$(PACKAGE_XML).tar.gz
TARBALL_OGG:=$(PACKAGE_OGG).tar.gz
TARBALL_OGGZ:=$(PACKAGE_OGGZ).tar.gz
TARBALL_VORBIS:=$(PACKAGE_VORBIS).tar.gz
TARBALL_THEORA:=$(PACKAGE_THEORA).tar.gz
