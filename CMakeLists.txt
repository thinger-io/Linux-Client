cmake_minimum_required(VERSION 2.8.9)
project(thinger)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# check c++11 support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

OPTION(ENABLE_OPENSSL "Enable use of OpenSSL" ON)
OPTION(DAEMON "Build thinger client as daemon" OFF)
OPTION(EDISON "Enable build and install for Intel Edison" OFF)
OPTION(RASPBERRY "Enable build and isntall for Raspberry Pi" OFF)

# Find OpenSSL
IF(ENABLE_OPENSSL)
  if(APPLE) # point to the correct OpenSSL path (installed by homebrew)
      SET(OPEN_SSL 1)
      include_directories(/usr/local/opt/openssl/include)
      list(APPEND ADDITIONAL_LIBS /usr/local/opt/openssl/lib/libssl.a)
      list(APPEND ADDITIONAL_LIBS /usr/local/opt/openssl/lib/libcrypto.a)
  else()
      find_package(OpenSSL REQUIRED)
      if(OPENSSL_FOUND)
          SET(OPEN_SSL 1)
          include_directories(${OPENSSL_INCLUDE_DIR})
          list(APPEND ADDITIONAL_LIBS ${OPENSSL_LIBRARIES})
          message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
      else()
          SET(OPEN_SSL 0)
      endif()
  endif()
ELSE()
  SET(OPEN_SSL 0)
ENDIF()

set(SOURCE_FILES src/main.cpp)

# set OpenSSL if available
add_definitions( -DOPEN_SSL=${OPEN_SSL} )

# Support for WiringPi on Raspberry
if(RASPBERRY)
    find_package(WiringPi)
    if(WiringPi_FOUND)
        include_directories(${WiringPi_INCLUDE_DIR})
        list(APPEND ADDITIONAL_LIBS ${WiringPi_LIBRARIES})
    endif()
endif()

if(DAEMON)
    # daemon service that can be installed as a linux service
    add_executable(thinger ${SOURCE_FILES})
    target_link_libraries(thinger ${ADDITIONAL_LIBS})
    set_target_properties(thinger PROPERTIES COMPILE_DEFINITIONS "DAEMON=1")

    if(EDISON)
        install(TARGETS thinger RUNTIME DESTINATION bin)
        install(FILES "${CMAKE_SOURCE_DIR}/install/intel_edison/thinger.service" DESTINATION /lib/systemd/system)
    elseif(RASPBERRY)
        install(TARGETS thinger RUNTIME DESTINATION bin)
        install(FILES "${CMAKE_SOURCE_DIR}/install/raspberry/thinger" DESTINATION /etc/init.d)
    elseif(MAC)

    endif()
else()
    add_executable(thinger ${SOURCE_FILES})
    target_link_libraries(thinger ${ADDITIONAL_LIBS})
    set_target_properties(thinger PROPERTIES COMPILE_DEFINITIONS "DAEMON=0")
endif()


