cmake_minimum_required(VERSION 3.14)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
	# sometimes people build without specifying build type, then get surprised how slow it is...
	message(FATAL_ERROR "Set CMAKE_BUILD_TYPE to either Release or Debug")
endif()

if (UNIX)
	# we want debug symbols even with Release builds
	string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT "-g -ffast-math")
	string(APPEND CMAKE_C_FLAGS_RELEASE_INIT "-g -ffast-math")
elseif(MSVC)
	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

option(TDM_THIRDPARTY_ARTEFACTS "Use prebuilt artefacts at ThirdParty/artefacts?" ON)

# find ordinary cmake includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/sys/cmake")

include(CMakeDependentOption)
include(ucm)
include(PrecompiledHeader)
include(SvnVersion)
include(CheckCXXCompilerFlag)
include(CheckPIESupported)

project(TheDarkMod)
# note: must be after project: https://gitlab.kitware.com/cmake/cmake/-/issues/14983
check_pie_supported()
if(TDM_THIRDPARTY_ARTEFACTS)
	include(ThirdParty/use_artefacts.cmake)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_dependent_option(WITH_TOOLS "Compile with Tools" ON "MSVC" OFF)
set(GAME_DIR "${PROJECT_SOURCE_DIR}/../darkmod" CACHE STRING "The directory where the game assets are stored")
option(COPY_EXE "Copy executable to GAME_DIR after build" ON)
option(ENABLE_TRACY "Enable tracy profiling support. Needs to be activated ingame by enabling the cvar com_enableTracing." ON)

if (UNIX)
	# configure sensible warnings
	add_compile_options(
			-Wno-unknown-pragmas            # MSVC specific pragmas
			-Wno-unused-variable            # too many
			-Wno-unused-but-set-variable
			-Wno-sign-compare               # in particular, int/size_t conversions
			-Wno-format-security            # format string is often composed dynamically
			-Woverloaded-virtual            # almost-overriding virtual functions is dangerous for us
			-Wsuggest-override              # recommend using "override" keyword
	)
	# configure other compile options
	add_compile_options(
			-pipe
			-fno-strict-aliasing
			-fno-omit-frame-pointer         # crashdumps!
			-fmessage-length=0
			-fvisibility=hidden
			-fno-unsafe-math-optimizations
			-std=c++14
	)
	# enable SSE2 if supported (mainly 32-bit x86)
	check_cxx_compiler_flag(-msse2 MSSE2_SUPPORTED)
	if (MSSE2_SUPPORTED)
		add_compile_options(-msse2)
	endif()

	# TODO: do we really want to keep using this
	# use old ABI for std::string and std::list to allow TDM to run on distros with old
	# glibcxx (e.g. Ubuntu 14.04). the old ABI is not fully compliant with C++11 standard.
	add_compile_definitions("_GLIBCXX_USE_CXX11_ABI=0")
	# ensure threading support in X
	add_compile_definitions(XTHREADS)
elseif (MSVC)
	add_compile_definitions("WIN32" _WINDOWS
			_CRT_SECURE_NO_DEPRECATE
			_CRT_NONSTDC_NO_DEPRECATE
			_ALLOW_KEYWORD_MACROS
			NO_WARN_MBCS_MFC_DEPRECATION
			_MBCS
	)
	# disable certain warnings
	add_compile_options(/wd4127 /wd4458 /wd4267)
	add_compile_options(/MP /Gm-)
	ucm_add_flags(/GS- /Gy /Zi /GF /FC /Oi /Oy /Ot CONFIG Release)
	ucm_add_linker_flags(EXE /LARGEADDRESSAWARE /NXCOMPAT /SAFESEH:NO /STACK:16777216,16777216)
	set(MSSE2_SUPPORTED 1)
	if (CMAKE_SIZEOF_VOID_P EQUAL 4)
		add_compile_options(/arch:SSE2)
	endif()
	ucm_add_linker_flags(EXE "/DEBUG" CONFIG Release)
endif ()

add_executable(TheDarkMod)

get_svn_revision(SVNVERSION)
configure_file("idlib/svnversion.cmake.h" "${PROJECT_SOURCE_DIR}/idlib/svnversion.h" @ONLY)

# gather list of source files
ucm_add_dirs(
		"cm"
		"framework"
		"game"
		"idlib"
		"renderer"
		"sound"
		"tests"
		"TypeInfo"
		"ui"
		"tools/compilers"
		TO TDM_SOURCE_FILES RECURSIVE
)
ucm_add_files(
	"sys/sys_local.cpp"
	"sys/sys_local.h"
	"sys/sys_public.h"
	"sys/sys_padinput.cpp"
	"sys/sys_padinput.h"
	"sys/sys_cpu.cpp"
TO TDM_SOURCE_FILES)

if (WITH_TOOLS)
	ucm_add_dirs("tools" TO TDM_SOURCE_FILES RECURSIVE)
	ucm_remove_files(
			"tools/edit_stub.cpp"
			"tools/decl/DialogEntityDefEditor.cpp"
			"tools/common/RenderBumpFlatDialog.cpp"
			"tools/guied/GEWindowWrapper_stub.cpp"
			FROM TDM_SOURCE_FILES
	)
else ()
	ucm_add_files("tools/guied/GEWindowWrapper_stub.cpp" TO TDM_SOURCE_FILES)
	add_compile_definitions(NO_MFC)
endif ()

if (UNIX)
	ucm_add_dirs("sys/linux" "sys/posix" TO TDM_SOURCE_FILES RECURSIVE)
	ucm_remove_directories("idlib/sys/win32" FROM TDM_SOURCE_FILES)
	ucm_remove_files(
			"renderer/backend/qgl/glad_wgl.c"
			"sys/linux/dedicated.cpp"
			"sys/linux/test_scheduler.c"
			"sys/posix/platform_osx.cpp"
			FROM TDM_SOURCE_FILES
	)
elseif (WIN32)
	ucm_add_dirs("sys/win32" TO TDM_SOURCE_FILES RECURSIVE)
	ucm_add_files("sys/win32/rc/doom.rc" TO TDM_SOURCE_FILES)
	ucm_remove_directories("idlib/sys/posix" FROM TDM_SOURCE_FILES)
	ucm_remove_files("renderer/backend/qgl/glad_glx.c" FROM TDM_SOURCE_FILES)
endif ()

# blacklist dead code files that unfortunately still hang around the repository
ucm_remove_directories("framework/async" FROM TDM_SOURCE_FILES)
ucm_remove_files(
		"TypeInfo/main.cpp"
		"game/Game_network.cpp"
		"game/MultiplayerGame.cpp"
		"game/gamesys/Callbacks.cpp"
		"idlib/math/Simd_AltiVec.cpp"
		"sys/win32/win_gamma.cpp"
		FROM TDM_SOURCE_FILES
)

target_sources(TheDarkMod PRIVATE ${TDM_SOURCE_FILES})

if (MSVC)
	ucm_add_dirs("sys/msvc/natvis" TO TDM_NATVIS_FILES RECURSIVE ADDITIONAL_EXT "natvis")
	target_sources(TheDarkMod INTERFACE ${TDM_NATVIS_FILES})
endif ()

# configure precompiled header
# note TypeInfo.cpp must be excluded from precompiled header because it uses some dirty hacks
add_precompiled_header(TheDarkMod
		"idlib/precompiled.h" "idlib/precompiled.cpp" "precompiled.h"
		EXCLUDE
		"game/gamesys/TypeInfo.cpp"
		"idlib/Token.cpp"
		"sys/win32/win_shared.cpp"
		"sys/win32/rc/CreateResourceIDs.cpp"
		"framework/TracingEmbeddedSourceCode.cpp"
)

add_compile_definitions(
		"ExtLibs="
)
if (ENABLE_TRACY)
	add_compile_definitions(
			"$<$<CONFIG:RELEASE>:TRACY_ENABLE>"
			TRACY_DELAYED_INIT
			TRACY_MANUAL_LIFETIME
			TRACY_ON_DEMAND
			TRACY_NO_BROADCAST
			TRACY_ONLY_LOCALHOST
	)
endif()

if (UNIX)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set_target_properties(TheDarkMod PROPERTIES OUTPUT_NAME "thedarkmod.x64")
	else ()
		set_target_properties(TheDarkMod PROPERTIES OUTPUT_NAME "thedarkmod.x86")
	endif ()
elseif (MSVC)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set_target_properties(TheDarkMod PROPERTIES OUTPUT_NAME "TheDarkModx64")
	else ()
		set_target_properties(TheDarkMod PROPERTIES OUTPUT_NAME "TheDarkMod")
	endif ()
endif ()
# ffmpeg dep was compiled without -fPIC, so need to disable PIE on 64bit builds
set_target_properties(TheDarkMod PROPERTIES POSITION_INDEPENDENT_CODE OFF)	# see also CMP0083
ucm_set_runtime(STATIC)

include_directories(
		"${PROJECT_SOURCE_DIR}"
		"${PROJECT_SOURCE_DIR}/idlib"
)

find_package(ZLIB REQUIRED CONFIG)
find_package(minizip REQUIRED CONFIG)
find_package(CURL REQUIRED CONFIG)
find_package(OpenAL REQUIRED CONFIG)
find_package(Ogg REQUIRED CONFIG)
find_package(Vorbis REQUIRED CONFIG)
find_package(JPEG REQUIRED CONFIG)
find_package(PNG REQUIRED CONFIG)
find_package(ffmpeg REQUIRED CONFIG)
find_package(pugixml REQUIRED CONFIG)
find_package(MbedTLS REQUIRED CONFIG)
find_package(glfw3 REQUIRED CONFIG)
find_package(doctest REQUIRED CONFIG)
find_package(tracy REQUIRED CONFIG)

target_link_libraries(TheDarkMod
		ZLIB::ZLIB
		minizip::minizip
		CURL::libcurl
		OpenAL::OpenAL
		vorbis::vorbis
		Ogg::ogg
		JPEG::JPEG
		PNG::PNG
		ffmpeg::ffmpeg
		glfw
		pugixml::pugixml
		doctest::doctest
		tracy::tracy
)

if (WIN32)
	target_link_libraries(TheDarkMod ws2_32 iphlpapi winmm bcrypt dinput8 dbghelp)
	set_target_properties(TheDarkMod PROPERTIES WIN32_EXECUTABLE TRUE)
elseif (UNIX)
	target_link_libraries(TheDarkMod pthread dl stdc++fs X11 Xext Xxf86vm)
	# strip debug symbols from executable and put them into a separate symbols file
	add_custom_command(
			TARGET TheDarkMod POST_BUILD
			COMMAND objcopy --only-keep-debug "$<TARGET_FILE:TheDarkMod>" "$<TARGET_FILE:TheDarkMod>.debug"
	)
	add_custom_command(
			TARGET TheDarkMod POST_BUILD
			COMMAND strip --strip-debug --strip-unneeded "$<TARGET_FILE:TheDarkMod>"
	)
	add_custom_command(
			TARGET TheDarkMod POST_BUILD
			COMMAND objcopy "--add-gnu-debuglink=$<TARGET_FILE:TheDarkMod>.debug" "$<TARGET_FILE:TheDarkMod>"
	)
endif ()

if (COPY_EXE)
	# copy produced executable to game directory '../darkmod'
	add_custom_command(
			TARGET TheDarkMod POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TheDarkMod> "${GAME_DIR}"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/glprogs" "${GAME_DIR}/glprogs"
	)
	if (UNIX)
		add_custom_command(
				TARGET TheDarkMod POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TheDarkMod>.debug "${GAME_DIR}"
		)
	endif()
endif ()
