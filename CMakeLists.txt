cmake_minimum_required (VERSION 3.22...5.1.0)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
	cmake_policy(SET CMP0091 NEW)
endif()

project(chimera
	VERSION 3.0.0
	DESCRIPTION "X11/Athena World-Wide Web Client"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 90)

include(FetchContent)
include(CheckLibraryExists)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GNUInstallDirs)

set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/built")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build the library as a shared (dynamically-linked) " OFF)
option(BUILD_NATIVE_XAW "Linux: Build using Native X11 Athena Widget Toolkit " ON)

if (WIN32 OR APPLE)
    message(FATAL_ERROR "Linux Only!")
else()
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} -dumpmachine
      RESULT_VARIABLE RESULT
      OUTPUT_VARIABLE PROJECT__ARCH
      ERROR_QUIET
    )
    if (RESULT)
      message(FATAL_ERROR "Failed to determine target architecture triplet: ${RESULT}")
    endif()

    string(REGEX MATCH "([^-]+).*" ARCH_MATCH ${PROJECT__ARCH})
    if (NOT CMAKE_MATCH_1 OR NOT ARCH_MATCH)
      message(FATAL_ERROR "Failed to match the target architecture triplet: ${PROJECT__ARCH}")
    endif()

    set(PROJECT__ARCH ${CMAKE_MATCH_1})
	message(STATUS "Target architecture - ${PROJECT__ARCH}")

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -rdynamic -no-pie -D USE_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format -fno-strict-aliasing -D NeedFunctionPrototypes")

	file(GLOB app_files
		${CMAKE_CURRENT_SOURCE_DIR}/chimera/*.c)

	file(GLOB proto_files
		${CMAKE_CURRENT_SOURCE_DIR}/proto/*.c)
	file(GLOB plain_files
		${CMAKE_CURRENT_SOURCE_DIR}/plain/*.c)
	file(GLOB mxw_files
		${CMAKE_CURRENT_SOURCE_DIR}/mxw/*.c)
	file(GLOB image_files
		${CMAKE_CURRENT_SOURCE_DIR}/image/*.c)
	file(GLOB html_files
		${CMAKE_CURRENT_SOURCE_DIR}/html/*.c)
	file(GLOB ext_files
		${CMAKE_CURRENT_SOURCE_DIR}/ext/*.c)
	file(GLOB common_files
		${CMAKE_CURRENT_SOURCE_DIR}/common/*.c)

	if(BUILD_SHARED_LIBS)
		add_library(${PROJECT_NAME} SHARED
		${proto_files} ${plain_files} ${mxw_files} ${image_files}
		${html_files} ${ext_files} ${common_files})
	else()
		add_library(${PROJECT_NAME} STATIC
		${proto_files} ${plain_files} ${mxw_files} ${image_files}
		${html_files} ${ext_files} ${common_files})
	endif()
	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

	find_package(opentls QUIET)
	if(NOT opentls_FOUND)
		FetchContent_Declare(opentls
			URL https://github.com/zelang-dev/opentls/archive/refs/heads/main.zip
			URL_MD5 b670326e488f0911aabd7dec311a4ceb
		)
		FetchContent_MakeAvailable(opentls)
	endif()

	find_package(TIFF REQUIRED)
	find_package(PNG REQUIRED)
	find_package(JPEG REQUIRED)
	find_package(X11 REQUIRED COMPONENTS Xmu Xt)
	find_package(OpenGL REQUIRED COMPONENTS GLX)
	if(BUILD_NATIVE_XAW)
		find_library(X11_Xaw_LIB Xaw)
	else()
		##file(GLOB Xaw95_files
		#	${CMAKE_CURRENT_SOURCE_DIR}/src/Linux/Xaw95/*.c)
		if(BUILD_SHARED_LIBS)
		#	add_library(Xaw95 SHARED ${Xaw95_files})
		else()
		#	add_library(Xaw95 STATIC ${Xaw95_files})
		endif()
		#target_include_directories(Xaw95
			#	PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
			#	PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
	endif(BUILD_NATIVE_XAW)

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}
		$<INSTALL_INTERFACE:${OPENSSL_INCLUDE_DIR})

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${TIFF_INCLUDE_DIRS}
		$<INSTALL_INTERFACE:${TIFF_INCLUDE_DIRS})

	if(BUILD_NATIVE_XAW)
		target_compile_definitions(${PROJECT_NAME}
			PUBLIC X11_WINDOW_ICON="${CMAKE_SOURCE_DIR}/resources/icon_32x32.xpm"
			PUBLIC NATIVE_XAW=1)
		target_link_libraries(${PROJECT_NAME}
			Xaw X11::Xmu X11::Xt Xpm X11::X11 OpenGL::GL OpenGL::GLU
			${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} opentls crypto dl m
			TIFF::TIFF PNG::PNG JPEG::JPEG)
	else()
		target_compile_definitions(${PROJECT_NAME}
			PUBLIC X11_WINDOW_ICON="${CMAKE_SOURCE_DIR}/resources/icon_32x32.xpm")
		target_link_libraries(${PROJECT_NAME}
			Xaw95 X11::Xmu X11::Xt Xpm X11::X11 OpenGL::GL OpenGL::GLU
			${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} opentls crypto dl m
			TIFF::TIFF PNG::PNG JPEG::JPEG)
	endif(BUILD_NATIVE_XAW)
endif()

add_library(CHIMERA::WEB ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME ${PROJECT_NAME}
	ARCHIVE_OUTPUT_NAME ${PROJECT_NAME}
	EXPORT_NAME ${PROJECT_NAME}
	VERSION  ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(NOT CMAKE_VERSION VERSION_LESS 3.27.0)
	set_target_properties(${PROJECT_NAME} PROPERTIES DLL_NAME_WITH_SOVERSION TRUE)
endif()

add_executable(${PROJECT_NAME}.app ${app_files})
target_include_directories(${PROJECT_NAME}.app
	PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/chimera>
	PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(${PROJECT_NAME}.app ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}.app PROPERTIES
	EXPORT_NAME ${PROJECT_NAME}.app
    OUTPUT_NAME ${PROJECT_NAME}.app
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

set(_fmt TGZ)
if(WIN32)
  set(_fmt ZIP)
endif()

set(CPACK_GENERATOR ${_fmt})
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_NUGET_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    CACHE PATH "Location of header files" )

set(SYSCONFIG_INSTALL_DIR ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    CACHE PATH "Location of configuration files" )

configure_package_config_file(${PROJECT_NAME}Config.cmake.in
  	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/"${PROJECT_NAME}"
	PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Package")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})

set(CPACK_PACKAGE_CONTACT "lstubbs@zelang.dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lawrence Stubbs <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${PROJECT__ARCH}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libglx-dev, libglu1-mesa-dev, libx11-dev, libxt-dev")

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "${PROJECT_NAME}")
set(CPACK_COMPONENT_LIBRARIES_DESCRIPTION ${PROJECT_DESCRIPTION})

set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "https://zelang-dev.github.io/${PROJECT_NAME}/")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "${PROJECT__ARCH}")
set(CPACK_RPM_PACKAGE_REQUIRES "mesa-libGL-devel, mesa-libGLU-devel, libX11-devel, libXt-devel")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" )

set(EXPORTER ${PROJECT_NAME})
install(
	TARGETS ${EXPORTER}
	EXPORT "${PROJECT_NAME}Targets"
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
	EXPORT "${PROJECT_NAME}Targets"
	FILE "${PROJECT_NAME}Targets.cmake"
	NAMESPACE CHIMERA::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(TARGETS ${PROJECT_NAME}.app DESTINATION bin)
install(DIRECTORY include DESTINATION include)
set(CPACK_INSTALL_CMAKE_CONFIGURATIONS Release)
