project( "micasa" )

cmake_minimum_required( VERSION 2.8.7 )

include( ExternalProject )
include( FindPackageHandleStandardArgs )
include( CheckIncludeFiles )

#
# Set sources directory.
# NOTE: for optional hardware or plugin support sources are added together later on.
#
set( SRC
	# libraries
	lib/mongoose/mongoose.c
	lib/v7/v7.c

	# main
	src/micasa.cpp
	src/Utils.cpp
	src/Arguments.cpp
	src/Scheduler.cpp
	src/Controller.cpp
	src/Logger.cpp
	src/Network.cpp
	src/Serial.cpp
	src/WebServer.cpp
	src/Hardware.cpp
	src/Device.cpp
	src/User.cpp
	src/Settings.cpp
	src/Database.cpp

	# hardware
	src/hardware/RFXCom.cpp
	src/hardware/SolarEdge.cpp
	src/hardware/SolarEdgeInverter.cpp
	src/hardware/WeatherUnderground.cpp
	src/hardware/HarmonyHub.cpp
	src/hardware/Dummy.cpp
	src/hardware/Telegram.cpp

	# devices
	src/device/Switch.cpp
	src/device/Counter.cpp
	src/device/Level.cpp
	src/device/Text.cpp
)

#
# Make sure thread support is available.
#
find_package( Threads REQUIRED )

#
# Set debug mode.
#
option( DEBUG "Build debug release" NO )
if( DEBUG )
	message( STATUS "Building DEBUG release" )
	set( CMAKE_BUILD_TYPE Debug )
	add_definitions( -D_DEBUG )
endif()

#
# Set default DATADIR location
#
set(DATADIR "./" CACHE STRING "Data directory")
message( STATUS "Using ${DATADIR} as datadir" )
add_definitions( -D_DATADIR="${DATADIR}" )

#
# Raspberry Pi options.
#
option( INCLUDE_LINUX_I2C "Include I2C support" YES )
if( INCLUDE_LINUX_I2C )
	CHECK_INCLUDE_FILES( "sys/types.h;linux/i2c-dev.h;linux/i2c.h" HAVE_LINUX_I2C_H )
	if( HAVE_LINUX_I2C_H )
		message( STATUS "Building with I2C support" )
		add_definitions( -D_WITH_LINUX_I2C )
	else()
		message( WARNING "I2C support disabled: headers not found!" )
	endif( HAVE_LINUX_I2C_H )
endif( INCLUDE_LINUX_I2C )

option( INCLUDE_SPI "Include SPI support" YES )
if( INCLUDE_SPI )
	CHECK_INCLUDE_FILES( "sys/types.h;linux/spi/spidev.h" HAVE_LINUX_SPI_H )
	IF( HAVE_LINUX_SPI_H )
		message( STATUS "Building with SPI support" )
		add_definitions( -D_WITH_LINUX_SPI )

		set( SRC ${SRC}
			src/hardware/PiFace.cpp
			src/hardware/PiFaceBoard.cpp
		)
	else()
		message(WARNING "SPI support disabled: headers not found!")
	endif( HAVE_LINUX_SPI_H )
endif( INCLUDE_SPI )

#
# Add and compile open-zwave project.
#
option( WITH_OPENZWAVE "Build with OpenZWave libraries" YES )
if( WITH_OPENZWAVE )
	message( STATUS "Build with OpenZWave libraries" )
	if( NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" )
		ExternalProject_Add(
			OpenZWave

			GIT_REPOSITORY "https://github.com/OpenZWave/open-zwave.git"
			# just fetch the most recent configs
			#GIT_TAG "v1.4"

			UPDATE_COMMAND ""
			PATCH_COMMAND ""
			TEST_COMMAND ""

			SOURCE_DIR "${CMAKE_SOURCE_DIR}/lib/open-zwave"
			PREFIX "${CMAKE_SOURCE_DIR}/lib/open-zwave-prefix"

			CONFIGURE_COMMAND ""
			BUILD_COMMAND make
			BUILD_IN_SOURCE 1
			INSTALL_COMMAND ""
		)
	else()
		ExternalProject_Add(
			OpenZWave

			GIT_REPOSITORY "https://github.com/OpenZWave/open-zwave.git"
			# just fetch the most recent configs
			#GIT_TAG "v1.4"

			UPDATE_COMMAND ""
			PATCH_COMMAND ""
			TEST_COMMAND ""

			SOURCE_DIR "${CMAKE_SOURCE_DIR}/lib/open-zwave"
			PREFIX "${CMAKE_SOURCE_DIR}/lib/open-zwave-prefix"

			CONFIGURE_COMMAND ""
			BUILD_COMMAND gmake
			BUILD_IN_SOURCE 1
			INSTALL_COMMAND ""
		)
	endif()

	set( OPENZWAVE_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib/open-zwave/cpp/src )
	set( OPENZWAVE_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib/open-zwave )

	add_library( OpenZWaveLib STATIC IMPORTED )
	set_target_properties( OpenZWaveLib PROPERTIES IMPORTED_LOCATION ${OPENZWAVE_LIB_DIR}/libopenzwave.a )

	add_definitions( -D_WITH_OPENZWAVE )

	set( SRC ${SRC}
		src/hardware/ZWave.cpp
		src/hardware/ZWaveNode.cpp
	)
endif()

#
# Add JSON.
#
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/lib/json )

#
# Add Mongoose Network Library.
# https://github.com/cesanta/mongoose
#
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/lib/mongoose )
add_definitions( -DMG_ENABLE_IPV6 -DMG_DISABLE_HTTP_DIGEST_AUTH -DMG_DISABLE_MD5 -DMG_DISABLE_HTTP_KEEP_ALIVE -DMG_ENABLE_BROADCAST )
set_source_files_properties( lib/mongoose/mongoose.c PROPERTIES COMPILE_FLAGS -DEXCLUDE_COMMON )

#
# Add v7 Javascript Engine.
# https://github.com/cesanta/v7
#
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/lib/v7 )
add_definitions( -DV7_ENABLE_FILE=ON )

#
# Set compiler options.
#
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -std=c++11" )
if( NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread" )
endif()
if( DEBUG )
#	AddressSanitizer has__interceptor_pthread_create problems.
#	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer" )
endif()

#
# Configure executable.
#
add_executable( micasa ${SRC} )

#
# Configure link libraries.
#
target_link_libraries( micasa ${PThreadLib})

#
# Check for Libudev.
#
find_path( LIBUDEV_INCLUDE_DIR NAMES libudev.h )
find_library( LIBUDEV_LIBRARY NAMES udev )
find_package_handle_standard_args( Libudev "Libudev not found" LIBUDEV_LIBRARY LIBUDEV_INCLUDE_DIR )
if( LIBUDEV_FOUND )
	include_directories( ${LIBUDEV_INCLUDE_DIR} )
	target_link_libraries( micasa ${LIBUDEV_LIBRARY} )
	add_definitions( -D_WITH_LIBUDEV )
endif( LIBUDEV_FOUND )

#
# OpenZWave needs libudev. While it is optional for micasa, it is not for OpenZWave.
#
if( WITH_OPENZWAVE )
	if( NOT LIBUDEV_FOUND )
		if( CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin" )
			message( STATUS "Building on FreeBSD or Darwin, libudev not needed for OpenZWave." )
			include_directories( ${OPENZWAVE_INCLUDE_DIR} )
			add_dependencies( micasa OpenZWave )

			#
			# Check for Libusb.
			#
			find_path( LIBUSB_INCLUDE_DIR NAMES usb.h )
			find_library( LIBUSB_LIBRARY NAMES usb )
			find_package_handle_standard_args( Libusb "Libusb not found" LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR )
			if( LIBUSB_FOUND )
				include_directories( ${LIBUSB_INCLUDE_DIR} )
				target_link_libraries( micasa OpenZWaveLib ${LIBUSB_LIBRARY} )
			else()
				message( FATAL_ERROR "Libusb not found on your system." )
			endif()

			if( CMAKE_SYSTEM_NAME STREQUAL "Darwin")
				set( IOKIT_LIBRARY "-framework IOKit -framework CoreFoundation" CACHE FILEPATH "IOKit framework" FORCE )
				target_link_libraries( micasa ${IOKIT_LIBRARY} )
			endif()
		else()
			message( FATAL_ERROR "Libudev not found on your system." )
		endif()
	else()
		include_directories( ${OPENZWAVE_INCLUDE_DIR} )
		add_dependencies( micasa OpenZWave )
		target_link_libraries( micasa OpenZWaveLib ${LIBUDEV_LIBRARY} )
	endif()
endif()

#
# Check for OpenSSL
#
option( WITH_OPENSSL "Build with OpenSSL libraries" YES )
if( WITH_OPENSSL )
	message( STATUS "Build with OpenSSL libraries" )
	find_package( OpenSSL )
	if( OPENSSL_FOUND )
		include_directories( ${OPENSSL_INCLUDE_DIRS} )
		target_link_libraries( micasa ${OPENSSL_LIBRARIES} )
		add_definitions( -D_WITH_OPENSSL -DMG_ENABLE_SSL )
	else()
		message( FATAL_ERROR "OpenSSL not found on your system." )
	endif( OPENSSL_FOUND )
else()
	message( STATUS "WARNING: Building without OpenSSL is for debugging purposes only." )
endif()

#
# Check for SQLite3
#
find_path( SQLITE3_INCLUDE_DIR NAMES sqlite3.h )
find_library( SQLITE3_LIBRARY NAMES sqlite3 )
find_package_handle_standard_args( SQLite3 "SQLite3 not found" SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR )
if( SQLITE3_FOUND )
	include_directories( ${SQLITE3_INCLUDE_DIR} )
	target_link_libraries( micasa ${SQLITE3_LIBRARY} )
else()
	message( FATAL_ERROR "SQLite3 not found on your system." )
endif( SQLITE3_FOUND )
