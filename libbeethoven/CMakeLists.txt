# libbeethoven — the public C++ API installed once globally to
# ${CMAKE_INSTALL_PREFIX}/lib + /include + /lib/cmake.
#
# Two cmake packages live underneath: `beethoven` (host: discrete + zynq
# side-by-side via COMPONENTS) and `beethoven_baremetal` (Cortex-M55).
# Toggled by -DBEETHOVEN_BAREMETAL.
#
# LIBBEETHOVEN_SOURCE_DIR is the absolute path to *this* directory —
# platforms/{host,baremetal}/CMakeLists.txt use it for `${_root}/include`,
# `${_root}/src`, etc. The top-level repo dir (CMAKE_SOURCE_DIR) holds
# the sibling runtime/, which Option B installs as a source package.

set(LIBBEETHOVEN_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

if(BEETHOVEN_BAREMETAL)
    add_subdirectory(platforms/baremetal)
else()
    add_subdirectory(platforms/host)
endif()
