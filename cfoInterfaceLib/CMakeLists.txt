cet_make_library(LIBRARY_NAME CFOInterface SOURCE
		CFO.cpp
		CFO_Registers.cpp
		CFO_Compiler.cpp
        LIBRARIES
		mu2e_pcie_utils::DTCInterface
		artdaq_core::artdaq-core_Utilities_ExceptionStackTrace
		artdaq_core_mu2e::artdaq-core-mu2e_Overlays
)

cet_make_exec(NAME CFORegDump SOURCE cfoRegDump.cc LIBRARIES mu2e_pcie_utils::CFOInterface)
cet_make_exec(NAME mu2eCompiler SOURCE mu2eCompiler.cc LIBRARIES mu2e_pcie_utils::CFOInterface)
cet_make_exec(NAME cfoUtil SOURCE util_main.cc LIBRARIES mu2e_pcie_utils::CFOInterface)

# install any documentation text files
FILE(INSTALL Commands.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/doc)

# Install_headers MUST BE FIRST...for some reason
install_headers()
install_source()

if(${BUILD_PYTHON_INTERFACE})
    message(-- mu2e_pcie_utils/cfoInterfaceLib: BUILD_PYTHON_INTERFACE=${BUILD_PYTHON_INTERFACE})
   find_package(SWIG REQUIRED)
   include(${SWIG_USE_FILE})
    add_subdirectory(python)
endif()

if(${BUILD_ROOT_INTERFACE})
  message(-- mu2e_pcie_utils/cfoInterfaceLib: BUILD_ROOT_INTERFACE=${BUILD_ROOT_INTERFACE})
  add_subdirectory(root)
endif()
