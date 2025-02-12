# ======================================================================
#  mu2e_pcie_utils main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/artdaq-demo/ups/setup_for_development
#  buildtool
# ======================================================================


cmake_minimum_required (VERSION 3.19 FATAL_ERROR)

find_package(cetmodules 3.19.02 REQUIRED)

project(mu2e-pcie-utils VERSION 3.03.01)

include(CetCMakeEnv)
cet_cmake_env()
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

cet_set_compiler_flags(DIAGS VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic -Wno-unused-function -Wno-unused-parameter
# EXTRA_C_FLAGS -std=gnu99
 )

string(TOUPPER ${CMAKE_BUILD_TYPE} BTYPE_UC )
#if( ${BTYPE_UC} MATCHES "DEBUG" )
#  cet_add_compiler_flags(-fsanitize=address)
#endif()

#cet_report_compiler_flags()

find_package(TRACE 3.17.05 REQUIRED)
find_package(messagefacility REQUIRED)
find_package(artdaq_core_mu2e REQUIRED)

# source
add_subdirectory(mu2e_driver)
add_subdirectory(mcs)

add_subdirectory(dtcInterfaceLib)
add_subdirectory(cfoInterfaceLib)

# tools
add_subdirectory(tools)

# Documentation (to comment out if not needed DOXYGEN locally)
if( $ENV{OTS_DOXY} MATCHES "DOIT" )
	add_subdirectory(doc)
endif()

cet_cmake_config()

