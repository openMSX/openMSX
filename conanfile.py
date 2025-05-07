from conan import ConanFile

class OpenMSXConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    def requirements(self):
        # Common dependencies
        self.requires("freetype/2.13.2")
        self.requires("opengl/system")
        self.requires("glew/2.2.0")
        self.requires("libpng/1.6.47")
        self.requires("ogg/1.3.5")
        self.requires("sdl/2.28.3")
        self.requires("sdl_ttf/2.24.0")
        self.requires("tcl/8.6.13")
        self.requires("theora/1.1.1")
        self.requires("vorbis/1.3.7")
        self.requires("zlib/1.3.1")

        # Linux-specific dependency
        if self.settings.os == "Linux":
            self.requires("libalsa/1.2.10")
