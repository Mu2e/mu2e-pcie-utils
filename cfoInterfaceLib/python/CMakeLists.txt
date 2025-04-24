# Include python
find_package(Python COMPONENTS Interpreter Development REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH}
					../
			       )

#set(CMAKE_SWIG_FLAGS "-v")
set_source_files_properties(cfoInterfaceLib.i PROPERTIES CPLUSPLUS ON USE_TARGET_INCLUDE_DIRECTORIES ON)

# Add swig module
swig_add_library(cfoInterfaceLib LANGUAGE python SOURCES cfoInterfaceLib.i)

# ${PROJECT_NAME} translates into mu2e_pcie_utils
swig_link_libraries(cfoInterfaceLib mu2e-pcie-utils::CFOInterface artdaq-core-mu2e::Overlays Python::Python)

# Configure setup.py and copy to output directory
set(SETUP_PY_IN  ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)
set(SETUP_PY_OUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
configure_file(${SETUP_PY_IN} ${SETUP_PY_OUT})

install(TARGETS cfoInterfaceLib DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cfoInterfaceLib.py DESTINATION lib)

# Declare install target for python
#install(TARGETS dtcInterfaceLib
#        COMMAND "${PYTHON_EXECUTABLE} setup.py"
#        COMPONENT swig-python)

# Install target to call setup.py
#add_custom_target(install-python
#        DEPENDS _dtcInterfaceLib
#        COMMAND python ${SETUP_PY_OUT} install)
