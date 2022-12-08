cmake_minimum_required(VERSION 3.12)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../../External/zlib/zconf.h.included" "${CMAKE_CURRENT_SOURCE_DIR}/../../External/zlib/zconf.h" COPYONLY)

file(GLOB_RECURSE SRC_FILES
	CONFIGURE_DEPENDS
	Src/*.cpp
	Src/*.c
	Src/*.hpp
	Src/*.h
)

add_library(OnlineJsonMods SHARED
	${SRC_FILES}
)

target_include_directories(OnlineJsonMods PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(OnlineJsonMods PRIVATE
	ZHMModSDK
	zlib
)
