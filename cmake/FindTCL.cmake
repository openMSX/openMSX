# FindTCL.cmake - minimal finder for the Tcl library and headers
# Supports Tcl 8.6 and future 9.0

find_path(TCL_INCLUDE_DIR
    NAMES tcl.h
    PATH_SUFFIXES tcl8.6 tcl9.0 tcl
    DOC "Path to Tcl include directory"
)

find_library(TCL_LIBRARY
    NAMES tcl8.6 tcl86 tcl9.0 tcl90 tcl
    DOC "Path to Tcl library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TCL
    REQUIRED_VARS TCL_LIBRARY TCL_INCLUDE_DIR
)

if(TCL_LIBRARY AND NOT TARGET tcl::tcl)
    add_library(tcl::tcl UNKNOWN IMPORTED)
    set_target_properties(tcl::tcl PROPERTIES
        IMPORTED_LOCATION "${TCL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TCL_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(TCL_INCLUDE_DIR TCL_LIBRARY)
