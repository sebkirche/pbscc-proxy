@echo off
regedit /ea tmp.reg HKEY_LOCAL_MACHINE\SOFTWARE\NSIS
for /F "delims=" %%i in ('findstr /C:"@=" tmp.reg') do echo set TTT%%i >tmp.cmd
call tmp.cmd

del tmp.reg >nul 2>nul
del tmp.cmd >nul 2>nul

set TTT@=%TTT@:\\=\%
set TTT@=%TTT@:~0,-2%\makensis.exe"

rem %TTT@% SvnProxy.nsi
%TTT@% pbsccsetup.nsi

del /Q *.zip >nul 2>nul

rem PKZIPC -add -lev=9 -path=none sccproxy.zip SccProxy.exe
svn info svn://luhome/projects/pbscc/trunk > svninfo.txt

PKZIPC -add -lev=9 -path=none pbsccsetup.zip pbsccsetup.exe svninfo.txt

del /Q *.exe >nul 2>nul
del /Q svninfo.txt >nul 2>nul
