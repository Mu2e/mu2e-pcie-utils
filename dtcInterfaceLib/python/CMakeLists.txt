# Include python
find_package(Python COMPONENTS Interpreter Development REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH}
                    ../			       
                    $ENV{$TRACE_INC} 
                    $ENV{$MESSAGEFACILITY_INC}
                    $ENV{CETLIB_INC}
                    $ENV{CETLIB_EXCEPT_INC}
		    )	

include(CetCMakeEnv)
cet_cmake_env()

set(CMAKE_SWIG_FLAGS "-v")
set_source_files_properties(dtcInterfaceLib.i PROPERTIES CPLUSPLUS ON USE_TARGET_INCLUDE_DIRECTORIES ON)

# Add swig module
swig_add_library(dtcInterfaceLib LANGUAGE python SOURCES dtcInterfaceLib.i)

# swig_link_libraries(dtc ${PROJECT_NAME} ${PYTHON_LIBRARIES})
# ${PROJECT_NAME} translates into mu2e_pcie_utils
swig_link_libraries(dtcInterfaceLib mu2e-pcie-utils::DTCInterface artdaq-core-mu2e::Overlays Python::Python)

# Configure setup.py and copy to output directory
set(SETUP_PY_IN ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)
set(SETUP_PY_OUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
configure_file(${SETUP_PY_IN} ${SETUP_PY_OUT})

install(TARGETS dtcInterfaceLib DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dtcInterfaceLib.py DESTINATION lib)

# Declare install target for python
#install(TARGETS dtcInterfaceLib
#        COMMAND "${PYTHON_EXECUTABLE} setup.py"
#        COMPONENT swig-python)

# Install target to call setup.py
#add_custom_target(install-python
#        DEPENDS _dtcInterfaceLib
#        COMMAND python ${SETUP_PY_OUT} install)
