# [ps5 port] CMake toolchain for building Ninja Gaiden II as a native PS5
# homebrew app. The whole build is in ps5/BUILD.md.
#
#   cmake -S . -B out/ps5 -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=ps5/cmake/ps5-toolchain.cmake \
#     -DPS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk \
#     -DREXSDK_DIR=/path/to/rexglue-sdk
#
# The PS5 payload SDK (mihawk-99's fork, as set up by Nazky's
# ps5-plume-triangle) provides clang for x86_64-sie-ps5, the headers, libc++ 18
# and the system library stubs.

if(NOT PS5_PAYLOAD_SDK)
  set(PS5_PAYLOAD_SDK "$ENV{PS5_PAYLOAD_SDK}")
endif()
if(NOT PS5_PAYLOAD_SDK OR NOT EXISTS "${PS5_PAYLOAD_SDK}/toolchain/prospero.cmake")
  message(FATAL_ERROR "Set PS5_PAYLOAD_SDK to the PS5 payload SDK root "
                      "(ps5-plume-triangle/.deps/native/ps5-payload-sdk)")
endif()
# try_compile projects need it too.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PS5_PAYLOAD_SDK)

include("${PS5_PAYLOAD_SDK}/toolchain/prospero.cmake")
set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "" FORCE)

# -march=znver2     the PS5's CPU is 8 Zen 2 cores.
# -femulated-tls    the PS5 ABI has no native TLS model for this code.
# -fexperimental-library
#                   libc++ 18 hides std::jthread / std::stop_token without it.
# No -mcmodel=large: tens of thousands of .ltext sections break the FSELF
#                   signer (found by the SvR 2011 port).
set(PS5_COMMON_FLAGS
  "-march=znver2 -femulated-tls -fno-stack-protector -fno-plt -fdenormal-fp-math=ieee -fvisibility-nodllstorageclass=default")
set(CMAKE_C_FLAGS_INIT "${PS5_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${PS5_COMMON_FLAGS} -fexperimental-library -frtti -fexceptions")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fexperimental-library")

# The display, the DualSense and audio come from the runtime's PS5 platform
# layer, not SDL: SDL is built with its dummy drivers only.
set(REX_PS5 ON CACHE BOOL "" FORCE)
foreach(_sdl_off X11 WAYLAND KMSDRM OPENGL OPENGLES OSS ALSA PULSEAUDIO PIPEWIRE JACK SNDIO
                 HIDAPI DBUS IBUS LIBUDEV VULKAN OFFSCREEN)
  set(SDL_${_sdl_off} OFF CACHE BOOL "" FORCE)
endforeach()
# SDL's .note.dlopen (its optional Linux libraries) lands between the read-only
# data and .dynamic, where the FSELF converter puts the PS5 process parameters:
# with it there, ps5-native-tool fails "no room for PS5 process parameters".
set(SDL_DLOPEN_NOTES OFF CACHE BOOL "" FORCE)
set(SDL_DUMMYVIDEO ON CACHE BOOL "" FORCE)
set(SDL_DUMMYAUDIO ON CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
