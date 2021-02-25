@echo off
cd /D "%~dp0"

if not exist intermediate_update.bin^
          echo The 'intermediate_update.bin' file does not exists.      ^
          && echo Prepare application intermediate version as described ^
          && echo in the documentation.                                 ^
          && exit /b 1

if not exist ..\..\..\..\apps\fmn\build\zephyr\app_update.bin^
          echo The '../../../../apps/fmn/build/zephyr/app_update.bin' file   ^
          && echo does not exists. Prepare an application image as described ^
          && echo in the documentation.                                      ^
          && exit /b 1

if not exist build mkdir build                                       || exit /b
copy /y intermediate_update.bin build\                               || exit /b
copy /y ..\..\..\..\apps\fmn\build\zephyr\app_update.bin build\      || exit /b

python ..\..\..\ncsfmntools SuperBinary     ^
	SuperBinary.plist                   ^
	--out-plist build\SuperBinary.plist ^
	--out-uarp build\SuperBinary.uarp   ^
	--metadata build\MetaData.plist     ^
	--payloads-dir build                ^
	--release-notes ..\ReleaseNotes     ^
	--mfigr2 skip                                                || exit /b

