cmake -G "Visual Studio 16 2019" -B build -S .
cmake --build build --config Release
build\src\App\Release\demo-app.exe
pause
