@echo off
set MYDIR=%~dp0
REM set ICPDIR=c:\development\instrumentcontrol\icp\labview modules\dae\service\x64
set ICPDIR=\\olympic\babylon5\public\freddie\isisdae\x64
echo Checking %ICPDIR% for updated DAE DLLs 
xcopy /i /q /d /y "%ICPDIR%\debug\*.dll" "%MYDIR%..\bin\windows-x64-debug"
xcopy /i /q /d /y "%ICPDIR%\debug\*.manifest" "%MYDIR%..\bin\windows-x64-debug"
xcopy /i /q /d /y "%ICPDIR%\debug\*.pdb" "%MYDIR%..\bin\windows-x64-debug"
xcopy /i /q /d /y "%ICPDIR%\release\*.dll" "%MYDIR%..\bin\windows-x64"
xcopy /i /q /d /y "%ICPDIR%\release\*.manifest" "%MYDIR%..\bin\windows-x64"
xcopy /i /q /d /y "%ICPDIR%\release\*.pdb" "%MYDIR%..\bin\windows-x64"

xcopy /i /q /d /y "%ICPDIR%\crt_manifest_for_dae3\*.*" "%MYDIR%..\bin\windows-x64-debug"