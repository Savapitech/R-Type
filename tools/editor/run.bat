@echo off
setlocal
set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

if not exist ".venv\Scripts\python.exe" (
  echo Creating .venv...
  python -m venv .venv || goto :fail
  ".venv\Scripts\python.exe" -m pip install -r tools\editor\requirements.txt || goto :fail
)

".venv\Scripts\python.exe" -m tools.editor %*
set "CODE=%ERRORLEVEL%"
popd
exit /b %CODE%

:fail
echo Failed to set up the editor environment.
popd
exit /b 1
