# Try to find the wiringPi library.
# This will define:
#
# WiringPi_FOUND       - wiringPi library is available
# WiringPi_INCLUDE_DIR - Where the wiringPi.h header file is
# WiringPi_LIBRARIES   - The libraries to link in.

find_library(WiringPi_LIBRARIES NAMES wiringPi PATHS
        /usr
        /usr/local
        /opt
        )

find_path(WiringPi_INCLUDE_DIR wiringPi.h PATHS
        /usr
        /usr/local
        /opt
        PATH_SUFFIXES
        )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WiringPi DEFAULT_MSG
        WiringPi_LIBRARIES
        WiringPi_INCLUDE_DIR
        )

mark_as_advanced(
        WiringPi_LIBRARIES
        WiringPi_INCLUDE_DIR
)