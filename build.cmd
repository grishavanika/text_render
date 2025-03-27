set VCPKG_ROOT=C:\Users\grish\dev\vcpkg
cmake -S . -B build ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
