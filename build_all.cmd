set VCPKG_ROOT=C:\Users\grish\dev\vcpkg
cmake -S . -B build-msvc-opengl-glfw ^
	-DKK_BUILD_RENDER_OPENGL=ON ^
	-DKK_BUILD_RENDER_VULKAN=OFF ^
	-DKK_BUILD_WND_WIN32=OFF ^
	-DKK_BUILD_WND_GLFW=ON ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake -S . -B build-msvc-opengl-win32 ^
	-DKK_BUILD_RENDER_OPENGL=ON ^
	-DKK_BUILD_RENDER_VULKAN=OFF ^
	-DKK_BUILD_WND_WIN32=ON ^
	-DKK_BUILD_WND_GLFW=OFF ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake -S . -B build-msvc-vulkan-glfw ^
	-DKK_BUILD_RENDER_OPENGL=OFF ^
	-DKK_BUILD_RENDER_VULKAN=ON ^
	-DKK_BUILD_WND_WIN32=OFF ^
	-DKK_BUILD_WND_GLFW=ON ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake -S . -B build-msvc-vulkan-win32 ^
	-DKK_BUILD_RENDER_OPENGL=OFF ^
	-DKK_BUILD_RENDER_VULKAN=ON ^
	-DKK_BUILD_WND_WIN32=ON ^
	-DKK_BUILD_WND_GLFW=OFF ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

cmake -S . -B build-clang-opengl-glfw ^
	-T ClangCL ^
	-DKK_BUILD_RENDER_OPENGL=ON ^
	-DKK_BUILD_RENDER_VULKAN=OFF ^
	-DKK_BUILD_WND_WIN32=OFF ^
	-DKK_BUILD_WND_GLFW=ON ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake -S . -B build-clang-opengl-win32 ^
	-T ClangCL ^
	-DKK_BUILD_RENDER_OPENGL=ON ^
	-DKK_BUILD_RENDER_VULKAN=OFF ^
	-DKK_BUILD_WND_WIN32=ON ^
	-DKK_BUILD_WND_GLFW=OFF ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake -S . -B build-clang-vulkan-glfw ^
	-T ClangCL ^
	-DKK_BUILD_RENDER_OPENGL=OFF ^
	-DKK_BUILD_RENDER_VULKAN=ON ^
	-DKK_BUILD_WND_WIN32=OFF ^
	-DKK_BUILD_WND_GLFW=ON ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake -S . -B build-clang-vulkan-win32 ^
	-T ClangCL ^
	-DKK_BUILD_RENDER_OPENGL=OFF ^
	-DKK_BUILD_RENDER_VULKAN=ON ^
	-DKK_BUILD_WND_WIN32=ON ^
	-DKK_BUILD_WND_GLFW=OFF ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
