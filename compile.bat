@set startdir=%cd%
@set derived=derived\3rdparty\src
@set wxlib=wxWidgets-3.2.1
@set visualstudio="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

@call %visualstudio% amd64

@echo -- log begin --

@echo -- Start: Catapult 3rd Party  --
cd ..
cd openmsx-wxcatapult
git pull
msbuild -p:Configuration="Unicode Release";Platform=x64 build\3rdparty\3rdparty.sln
set rootdir=%cd%

cd %rootdir%/%derived%\%wxlib%\build\msw\
nmake /f makefile.vc BUILD=release TARGET_CPU=X64 RUNTIME_LIBS=static

cd "%rootdir%\derived\x64-VC-Unicode Release\3rdparty\install\lib\"
xcopy "%rootdir%/%derived%\%wxlib%\lib\vc_x64_lib" "%cd%" /y
@echo -- End ----

@echo -- Start: Catapult Main  --
cd %rootdir%
msbuild -p:Configuration="Unicode Release";Platform=x64 build\msvc\wxCatapult.sln /m
@echo -- End --

cd %startdir%

@echo -- Start: openMSX 3rd part download --
call "python3" build\\thirdparty_download.py windows
@echo -- End --


@echo -- Start: openMSX 3rd part Compile --
msbuild -p:Configuration=Release;Platform=x64 build\3rdparty\3rdparty.sln /m 
@echo -- End --


@echo -- Start: openMSX Main Compile --
msbuild -p:Configuration=Release;Platform=x64 build\msvc\openmsx.sln /m 
@echo -- End --

@echo -- Start: openMSX MSI --
@call "build\\package-windows\\package.cmd" x64 release ..\\openmsx-wxcatapult
@echo -- End --
