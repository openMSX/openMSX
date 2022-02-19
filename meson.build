project(
    'openMSX', 'cpp',
    license: 'GPL-2.0-only',
    default_options: ['warning_level=3', 'cpp_std=c++17'],
    meson_version: '>=0.50.0'
)

subdir('src')

# Compile Flags
# =============

compiler = meson.get_compiler('cpp')

if compiler.get_argument_syntax() == 'gcc'
# We support GCC and Clang.

# Additional warnings:

add_project_arguments('-Wundef', language: 'cpp')
add_project_arguments('-Wshadow', language: 'cpp')
add_project_arguments('-Wunused-macros', language: 'cpp')
add_project_arguments('-Wdouble-promotion', language: 'cpp')
add_project_arguments('-Wmissing-declarations', language: 'cpp')
add_project_arguments('-Wold-style-cast', language: 'cpp')
add_project_arguments('-Wzero-as-null-pointer-constant', language: 'cpp')

# Disable warnings that cause too many false positives:

add_project_arguments('-Wno-pedantic', language: 'cpp')
add_project_arguments('-Wno-invalid-offsetof', language: 'cpp')

# Hardware descriptions can contain constants that are not used in the code
# but still useful as documentation.
# The flag exists for both GCC and Clang, but GCC doesn't enable it via -Wall.
# We'll disable it for both, just in case GCC auto-enables it in the future.
add_project_arguments('-Wno-unused-const-variable', language: 'cpp')

endif

# Dependencies
# ============

dep_sdl2 = dependency('SDL2')
dep_sdl2_ttf = dependency('SDL2_ttf')
dep_png = dependency('libpng')
dep_tcl = dependency('tcl', version: '>=8.6.0')
dep_threads = dependency('threads')
dep_zlib = dependency('zlib')

dep_gl = dependency('GL', required: get_option('glrenderer'))
dep_glew = dependency('glew', required: get_option('glrenderer'))

dep_ogg = dependency('ogg', required: get_option('laserdisc'))
dep_theora = dependency('theoradec', required: get_option('laserdisc'))
dep_vorbis = dependency('vorbis', required: get_option('laserdisc'))

if host_machine.system() == 'linux'
dep_alsa = dependency('alsa', required: get_option('alsamidi'))
else
dep_alsa = dependency('', required: false)
endif

# Components
# ==========

components = {
    'CORE':
        true,
    'GL':
        not get_option('glrenderer').disabled()
        and dep_gl.found() and dep_glew.found(),
    'LASERDISC':
        not get_option('laserdisc').disabled()
        and dep_ogg.found() and dep_theora.found() and dep_vorbis.found(),
    'ALSAMIDI':
        not get_option('alsamidi').disabled()
        and dep_alsa.found(),
}

# TODO: Subset the sources.

conf_components = configuration_data()
enabled_components = []
foreach name, enabled: components
    conf_components.set10('COMPONENT_' + name, enabled)
    if enabled
        enabled_components += name
    endif
endforeach
conf_components.set(
    'BUILD_COMPONENTS', '"' + ' '.join(enabled_components) + '"'
)
hdr_components = configure_file(
    output: 'components.hh',
    configuration: conf_components
)

# System Functions
# ================

conf_systemfuncs = configuration_data()
conf_systemfuncs.set10(
    'HAVE_FTRUNCATE',
    compiler.has_function('ftruncate', prefix: '#include <unistd.h>')
)
if host_machine.system() in ['darwin', 'openbsd']
    mmap_prefix = '\n'.join([
        '#include <sys/types.h>',
        '#include <sys/mman.h>'
    ])
else
    mmap_prefix = '#include <sys/mman.h>'
endif
conf_systemfuncs.set10(
    'HAVE_MMAP',
    compiler.has_function('mmap', prefix: mmap_prefix)
)
conf_systemfuncs.set10(
    'HAVE_NFTW',
    compiler.has_function('nftw', prefix: '#include <ftw.h>')
)
conf_systemfuncs.set10(
    'HAVE_POSIX_MEMALIGN',
    compiler.has_function('posix_memalign', prefix: '#include <stdlib.h>')
)
hdr_systemfuncs = configure_file(
    output: 'systemfuncs.hh',
    configuration: conf_systemfuncs
)

# Other Generated Headers
# =======================

prog_python = import('python').find_installation('python3')

hdr_version = custom_target(
    'version header',
    output: 'Version.ii',
    input: 'build/version2code.py',
    command: [prog_python, '@INPUT@', '@OUTPUT@'],
    build_always_stale: true,
)

abs_datadir = get_option('prefix') / get_option('datadir') / 'openmsx'

hdr_config = custom_target(
    'build info header',
    output: 'build-info.hh',
    input: 'build/buildinfo2code.py',
    command: [
        prog_python, '@INPUT@', '@OUTPUT@',
        host_machine.system(),
        host_machine.cpu_family(), # TODO: is cpu() more suitable?
        'devel', # OPENMSX_FLAVOUR
        abs_datadir
    ],
)

# Targets
# =======

main_exec = executable(
    'openmsx',
    main_sources, sources,
    hdr_version, hdr_config, hdr_components, hdr_systemfuncs,
    install: true,
    implicit_include_directories: false,
    include_directories: [incdirs, '.'],
    dependencies: [
        dep_alsa, dep_gl, dep_glew, dep_ogg, dep_png, dep_sdl2, dep_sdl2_ttf,
        dep_tcl, dep_theora, dep_threads, dep_vorbis, dep_zlib
    ],
)

objects = main_exec.extract_objects(sources)

test_exec = executable(
    'unittest',
    test_sources,
    hdr_version, hdr_config, hdr_components, hdr_systemfuncs,
    objects: objects,
    build_by_default: false,
    install: false,
    implicit_include_directories: false,
    include_directories: [incdirs, '.', 'Contrib/catch2'],
    dependencies: [
        dep_alsa, dep_gl, dep_glew, dep_ogg, dep_png, dep_sdl2, dep_sdl2_ttf,
        dep_tcl, dep_theora, dep_threads, dep_vorbis, dep_zlib
    ],
)

test('combined unit test', test_exec)
