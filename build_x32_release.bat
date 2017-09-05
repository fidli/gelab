@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
cls
set FILES="%cd%\sources\windows32_platform.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib" "Gdi32.lib"
set BASELIB="%cd%\baselib"
pushd build
del *.pdb
rem CANNOT USE SOME C++ FEATURES, std lib is ripped off (https://hero.handmade.network/forums/code-discussion/t/94)
call cl.exe /nologo /arch:IA32 /W2 /WX /EHa- /GS- /GR- /Od  /FS -D RELEASE /I %BASELIB% /Fegelab.exe %FILES%  /link /INCREMENTAL:NO /NODEFAULTLIB /SUBSYSTEM:CONSOLE %LIBS%

POPD