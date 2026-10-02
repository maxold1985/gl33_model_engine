@echo off
if not exist build\model_engine.exe (
    echo Execute build.bat primeiro.
    pause
    exit /b 1
)
build\model_engine.exe
