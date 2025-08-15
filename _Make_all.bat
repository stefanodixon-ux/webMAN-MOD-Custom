@echo off
mode con: cols=132 lines=45

set CELL_SDK=/c/msys/1.0/cell_4.50
set PS3SDK=/c/PSDK3v2
set PS3DEV=/c/PSDK3v2/ps3dev2
set WIN_PS3SDK=C:/PSDK3v2
SET PATH=%WIN_PS3SDK%/mingw/msys/1.0/bin;%WIN_PS3SDK%/mingw/bin;%PS3DEV%/ppu/bin;%PATH%;

set CHERE_INVOKING=1

cls

if exist .\webftp_server*.* del /q .\webftp_server*.*>nul

if not exist objs goto build

del /q objs\cobra\*.d>nul
del /q objs\cobra\*.o>nul
rd objs\cobra>nul

attrib -r objs\*.*

del /q objs\*.d>nul
del /q objs\*.o>nul
rd objs>nul

:build

cls
title [1/4] Building webftp_server_full.sprx...
copy .\flags\flags_full.h .\flags.h >nul
make>nul
if exist webftp_server.sprx ren webftp_server.sprx webftp_server_full.sprx

del /q webftp_server.elf>nul
del /q webftp_server.prx>nul
del /q webftp_server.sym>nul

attrib +r objs\libc.ppu.*
attrib +r objs\printf.ppu.*

del /q objs\*.d>nul
del /q objs\*.o>nul
rd objs>nul

cls
title [2/4] Building webftp_server_rebug_cobra_ps3mapi.sprx...
copy .\flags\flags_rebug_cobra_ps3mapi.h .\flags.h >nul
make>nul
if exist webftp_server.sprx ren webftp_server.sprx webftp_server_rebug_cobra_ps3mapi.sprx

del /q webftp_server.elf>nul
del /q webftp_server.prx>nul
del /q webftp_server.sym>nul

del /q objs\*.d>nul
del /q objs\*.o>nul

cls
title [3/4] Building webftp_server_lite.sprx...
copy .\flags\flags_lite.h .\flags.h >nul
make>nul
if exist webftp_server.sprx ren webftp_server.sprx webftp_server_lite.sprx

del /q webftp_server.elf>nul
del /q webftp_server.prx>nul
del /q webftp_server.sym>nul

del /q objs\*.d>nul
del /q objs\*.o>nul

cls
title [4/4] Building webftp_server_noncobra.sprx...
copy .\flags\flags_noncobra.h .\flags.h >nul
make>nul
if exist webftp_server.sprx ren webftp_server.sprx webftp_server_noncobra.sprx

del /q webftp_server.elf>nul
del /q webftp_server.prx>nul
del /q webftp_server.sym>nul

del /q objs\*.d>nul
del /q objs\*.o>nul

cls
del /q objs\cobra\*.d>nul
del /q objs\cobra\*.o>nul

rd objs\cobra>nul

attrib -r objs\*.*

del /q objs\*.d>nul
del /q objs\*.o>nul
rd objs>nul

copy .\flags\flags_full.h .\flags.h >nul

cls
dir *.sprx

echo press any key to copy and build pkg
pause>nul

_Make_pkg.bat
