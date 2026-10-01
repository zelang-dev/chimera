#[=======================================================================[

FindAthena
------------

Find the Athena library.

Imported Targets
^^^^^^^^^^^^^^^^

This module defines the following imported targets:

ATHENA::WIDGETS
    The Athena library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``ATHENA_FOUND``
    System has the Athena library.
``ATHENA_INCLUDE_DIR``
    The Athena include directory.
``ATHENA_LIBRARY``
    The Athena library.
``ATHENA_VERSION``
    This is set to $major.$minor.$revision (e.g. 2.6.8).

Hints
^^^^^

Set ATHENA_ROOT_DIR to the root directory of an Athena installation.

]=======================================================================]

# Find TLS Library
find_library(athena_LIBRARY
    NAMES
        libathena
)
mark_as_advanced(athena_LIBRARY)

# Find Include Path
find_path(athena_INCLUDE_DIR
    NAMES
		athena.h
)
mark_as_advanced(athena_INCLUDE_DIR)

include (FindPackageHandleStandardArgs)
# Set Find Package Arguments
find_package_handle_standard_args(athena
    FOUND_VAR athena_FOUND
    REQUIRED_VARS ATHENA_LIBRARY ATHENA_INCLUDE_DIR
    VERSION_VAR ATHENA_VERSION
    HANDLE_COMPONENTS
        FAIL_MESSAGE
        "Could NOT find Athena, try setting the path to Athena using the ATHENA_ROOT_DIR environment variable"
)

set(ATHENA_FOUND ${athena_FOUND})
set(ATHENA_LIBRARY ${ATHENA_LIBRARY})

# Athena Found
if(ATHENA_FOUND)
	set(ATHENA_INCLUDE_DIRS ${ATHENA_INCLUDE_DIR})
	set(ATHENA_LIBRARIES ${ATHENA_LIBRARY})
    if(NOT TARGET ATHENA::WIDGETS)
        add_library(ATHENA::WIDGETS UNKNOWN IMPORTED)
        set_target_properties(ATHENA::WIDGETS PROPERTIES
			IMPORTED_LOCATION "${ATHENA_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${ATHENA_INCLUDE_DIRS}"
        )
    endif()
endif(ATHENA_FOUND)
