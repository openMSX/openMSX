#ifndef TCL_HH
#define TCL_HH

#include <tcl.h>

#if TCL_MAJOR_VERSION < 9
    // Tcl8 uses 'int' for list size/index
    // Tcl9 uses 'Tcl_Size' which is a typedef for 'ptrdiff_t'
    using Tcl_Size = int;
#endif

#endif
