cmake_minimum_required(VERSION 2.6)

set(LIBENCODER_STANDALONE_BUILD ON CACHE BOOL INTERNAL)
set(LIBENCODER_STATIC_BUILD OFF CACHE BOOL INTERNAL)

if (LIBENCODER_STANDALONE_BUILD)
project(libencoder CXX C)
endif()

if(LIBENCODER_STATIC_BUILD)
	set(ENCODER_LIB_TYPE STATIC)
	#add_definitions(-DBUILD_STATIC_NO_EXPORT)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
	set(ENCODER_LIB_TYPE SHARED)
endif()


include(CheckCXXCompilerFlag)
include(CheckLibraryExists)

if (MSVC)
	set(CompilerFlags
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_MINSIZEREL
			CMAKE_CXX_FLAGS_RELWITHDEBINFO
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_RELEASE
			CMAKE_C_FLAGS_MINSIZEREL
			CMAKE_C_FLAGS_RELWITHDEBINFO
		)

	foreach(CompilerFlag ${CompilerFlags})
	string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()

	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()


CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden COMPILER_HAS_VISIBILITY_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-std=c++11 LIBCXX_HAS_STDCXX11_FLAG)
CHECK_CXX_COMPILER_FLAG(-s LIBCXX_HAS_STRIP_FLAG)

if (LIBCXX_HAS_STRIP_FLAG)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

if (${COMPILER_HAS_VISIBILITY_INLINE_HIDDEN})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif()

if (${COMPILER_HAS_VISIBILITY_HIDDEN})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

if (NOT ANDROID AND NOT MSVC)
	if(LIBCXX_HAS_STDCXX11_FLAG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	else()
		CHECK_CXX_COMPILER_FLAG(-std=c++0x LIBCXX_HAS_STDCXX0X_FLAG)
		if(LIBCXX_HAS_STDCXX0X_FLAG)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
		else()
			if(NOT CMAKE_CXX_COMPILER MATCHES "icc")
				message(FATAL "need at least gcc 4.4.7 or clang 3.2")
			endif(NOT CMAKE_CXX_COMPILER MATCHES "icc")
		endif()
	endif(LIBCXX_HAS_STDCXX11_FLAG)
	# endif() # if(NOT CMAKE_CXX_COMPILER MATCHES "icc")
endif(NOT ANDROID AND NOT MSVC)

if (WIN32)
	add_definitions(-DNOMINMAX)
	add_definitions (-D_WIN32_WINNT=0x0501 -D_SCL_SECURE_NO_WARNINGS)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


find_package(Threads REQUIRED)

if(WIN32)
	add_definitions(-DBOOST_ALL_STATIC_LINK)
	add_definitions(-DBOOST_THREAD_USE_LIB)
	add_definitions(-DBOOST_FILESYSTEM_STATIC_LINK)
	add_definitions(-DBOOST_ALL_STATIC_LINK)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

find_package(Boost REQUIRED COMPONENTS regex system thread chrono filesystem atomic)

if(WIN32)

	get_filename_component(CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        SET(FFMPEG_avcodec_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/include)
        SET(FFMPEG_avfilter_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/include)
        SET(FFMPEG_avformat_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/include)
        SET(FFMPEG_avutil_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/include)
        SET(FFMPEG_swresample_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/include)
        SET(FFMPEG_swscale_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/include)

        SET(FFMPEG_avcodec_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/lib/avcodec.lib)
        SET(FFMPEG_avfilter_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/lib/avfilter.lib)
        SET(FFMPEG_avformat_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/lib/avformat.lib)
        SET(FFMPEG_avutil_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/lib/avutil.lib)
        SET(FFMPEG_swresample_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/lib/swresample.lib)
        SET(FFMPEG_swscale_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win64-dev/lib/swscale.lib)
    else()
        SET(FFMPEG_avcodec_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/include)
        SET(FFMPEG_avfilter_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/include)
        SET(FFMPEG_avformat_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/include)
        SET(FFMPEG_avutil_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/include)
        SET(FFMPEG_swresample_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/include)
        SET(FFMPEG_swscale_INCLUDE_DIR ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/include)

        SET(FFMPEG_avcodec_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/lib/avcodec.lib)
        SET(FFMPEG_avfilter_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/lib/avfilter.lib)
        SET(FFMPEG_avformat_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/lib/avformat.lib)
        SET(FFMPEG_avutil_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/lib/avutil.lib)
        SET(FFMPEG_swresample_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/lib/swresample.lib)
        SET(FFMPEG_swscale_LIBRARY ${CURRENT_SOURCE_DIR}/third_party/ffmpeg-win32-dev/lib/swscale.lib)
    endif()
endif()

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil avformat swresample swscale)

link_directories(${Boost_LIBRARY_DIRS})

add_library(libencoder ${ENCODER_LIB_TYPE} include/export_import_def.hpp  include/libencoder.hpp  include/libencoder_api.hpp
	src/encoder.cpp src/encoder.hpp src/wrapper.cpp src/ffmpeg_encoder.cpp src/ffmpeg_encoder.hpp)

set_target_properties(libencoder
		PROPERTIES
		OUTPUT_NAME encoder
		PREFIX "lib"
		CLEAN_DIRECT_OUTPUT 1)

target_include_directories(libencoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_include_directories(libencoder PRIVATE ${Boost_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(libencoder ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if(NOT WIN32)
target_link_libraries(libencoder ${Boost_LIBRARIES})
endif()

add_executable(encoder test/main.cpp)
target_include_directories(encoder PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(encoder libencoder)

#install(TARGETS libencoder LIBRARY DESTINATION lib)

