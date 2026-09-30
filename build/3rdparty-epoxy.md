# libepoxy in the openMSX 3rd party build

openMSX builds libepoxy itself wherever it builds its 3rd party libraries
statically. libepoxy only ships a meson build, while openMSX's 3rd party
system is make, plus Visual C++ projects on Windows. Rather than requiring
meson and ninja, the build is reproduced here. That is feasible because it
is small: run `src/gen_dispatch.py` on the Khronos XML registry files to
generate the dispatch tables and their headers, compile those together with
a few hand-written sources, archive the result. There is no feature probing;
which dispatch APIs are built is a fixed choice per platform.

## Where everything lives

| File | Role |
|---|---|
| `build/packages.py`, class `EPOXY` | version, tarball name, size and SHA-256 |
| `build/libraries.py`, class `EPOXY` | how openMSX probes for and links the library |
| `build/3rdparty.mk` | the per-OS list of dispatch APIs (`EPOXY_APIS`) and the rule that installs the makefile below as the package's `Makefile` |
| `build/3rdparty-epoxy.mk` | the make-driven build: generate, compile, archive, install |
| `build/3rdparty/libepoxy-<version>.diff` | patches applied after extraction, on every platform |
| `build/3rdparty/libepoxy.vcxproj` and `.filters` | the Visual C++ build, doing the same steps as the makefile |
| `build/3rdparty/libepoxy-config.h` | hand-written `config.h` for the Visual C++ build |
| `build/3rdparty/3rdparty.props`, `LibNameEpoxy` | the name of the extracted source directory |

### Make-driven platforms (Linux, macOS, mingw, ...)

`build/3rdparty.mk` copies `build/3rdparty-epoxy.mk` into the package's build
directory and runs it with `SRC_DIR`, `INSTALL_DIR`, `PYTHON`, `EPOXY_APIS`
and `CFLAGS` set; `CC`, `AR` and `RANLIB` come from the environment. The
makefile writes its own `config.h` from `EPOXY_APIS`, generates one dispatch
source and header per API, compiles `dispatch_common.c` plus one
`dispatch_<api>.c` per API other than `gl`, and installs `libepoxy.a` with
the public and generated headers.

`EPOXY_APIS` is `gl wgl` for mingw, `gl` for macOS and `gl glx egl`
elsewhere. This mirrors the defaults of libepoxy's own `meson.build`.

### Visual C++

`libepoxy.vcxproj` copies `libepoxy-config.h` and the public headers into
place with custom build steps, runs `gen_dispatch.py` on `registry/gl.xml`
and `registry/wgl.xml`, and compiles `dispatch_common.c`, `dispatch_wgl.c`
and the two generated sources. `EPOXY_PUBLIC=extern` is defined both here
and in `build/msvc/openmsx.vcxproj`, because the header otherwise declares
the symbols `dllimport`, which is wrong for a static library.

## The patches

`libepoxy-<version>.diff` currently carries three changes. Check whether
each is still needed when updating.

- `src/gen_dispatch.py`: drop the `uintptr_t` cast on `GLhandleARB`
  arguments. On macOS `GLhandleARB` is a pointer type and recent clang
  rejects the resulting int-to-pointer conversion. Taken from LibreOffice
  (`external/epoxy/Wint-conversion.patch`,
  <https://gerrit.libreoffice.org/c/core/+/138407>).
- `src/dispatch_wgl.c`: initialise the per-thread dispatch tables on first
  use as well as from `DllMain`, which never runs for a static library.
  From <https://github.com/anholt/libepoxy/pull/304>, still open upstream
  at the time of writing.
- `src/dispatch_common.c`: on MSVC, skip the check that `dlopen()` is not
  being called from inside the dynamic linker. The check relies on a
  `.CRT$XCU` constructor that does not survive linking libepoxy statically
  with MSVC, so it aborted at startup. mingw runs the constructor and keeps
  the check. openMSX-specific; LibreOffice works around the same problem
  differently in its `clang-cl.patch`.

## Updating to a new libepoxy release

1. In `build/packages.py`, set `version`, `fileLength` and the SHA-256 of the
   new tarball. Releases are at
   <https://download.gnome.org/sources/libepoxy/>.
2. In `build/3rdparty/3rdparty.props`, update `LibNameEpoxy`.
3. Rename the patch file to the new version. Extract the new tarball, check
   each hunk against the new sources and against upstream (a fix may have
   landed), drop what is no longer needed and regenerate the diff with
   `diff -ur` against a pristine copy.
4. Compare the new `src/meson.build` with `build/3rdparty-epoxy.mk`: the
   list of hand-written sources, the `gen_dispatch.py` invocation and the
   compile flags. Compare the top-level `meson.build` with the `EPOXY_APIS`
   table in `build/3rdparty.mk`.
5. Compare what the new `meson.build` writes to `config.h` for MSVC with
   `build/3rdparty/libepoxy-config.h`, and for the other platforms with the
   `config.h` rule in `build/3rdparty-epoxy.mk`.
6. If sources or public headers were added, removed or renamed, update the
   `ClCompile` and `CustomBuild` items in `libepoxy.vcxproj` and
   `libepoxy.vcxproj.filters`, and the `install` target in
   `build/3rdparty-epoxy.mk`.
7. Build on every platform: `make staticbindist` for Linux, macOS and
   mingw, and `build\3rdparty\3rdparty.sln` followed by
   `build\msvc\openmsx.sln` with Visual C++. A pull request runs all of
   these in CI.
