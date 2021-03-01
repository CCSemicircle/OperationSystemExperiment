@ECHO OFF

SET PATH=%SystemRoot%;%SystemRoot%\system32
SET PATH=%~dp0tools;%~dp0tools\vim;%PATH%

IF EXIST %~dp0Qemu (
    SET QEMUHOME=%~dp0Qemu
    SET PATH=%~dp0Qemu;%PATH%
)

IF EXIST %~dp0msys\2.0 SET PATH=%~dp0msys\2.0\usr\bin;%PATH%

set LC_ALL=C

%ComSpec%
