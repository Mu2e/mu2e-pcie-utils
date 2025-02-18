set(NODEJS_ADDON_NAME "DTC")

#set(CMAKE_VERBOSE_MAKEFILE ON)

cet_make_library(LIBRARY_NAME DTCInterface 
    SOURCE
      DTC.cpp
      DTCLibTest.cpp
      DTCSoftwareCFO.cpp
      DTC_Registers.cpp
      CFOandDTC_Registers.cpp
      CFOandDTC_DMAs.cpp
      mu2edev.cpp
      mu2esim.cpp
      otsStyleCoutMacros.cpp
    LIBRARIES PUBLIC 
      TRACE::MF
      artdaq_core_mu2e::artdaq-core-mu2e_Overlays
      Threads::Threads
)

cet_make_exec(NAME mu2eUtil SOURCE util_main.cc Mu2eUtil.cpp LIBRARIES mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME rocUtil SOURCE dcs_main.cc LIBRARIES mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME data_file_verifier SOURCE data_file_verifier.cc LIBRARIES  mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME binary_dump_to_DTC_file SOURCE binary_dump_to_DTC_file.cc LIBRARIES  mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME DTC_binary_file_compare SOURCE DTC_binary_file_compare.cc LIBRARIES  mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME mu2eRequestSender SOURCE requestSender.cc LIBRARIES mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME DTCRegDump SOURCE dtcRegDump.cc LIBRARIES mu2e_pcie_utils::DTCInterface)

cet_make_exec(NAME my_cntl SOURCE my_cntl.cc LIBRARIES mu2e_pcie_utils::DTCInterface)
cet_make_exec(NAME rick_clock_test SOURCE rick_clock_test.cc LIBRARIES mu2e_pcie_utils::DTCInterface)

# Install_headers MUST BE FIRST...for some reason
install_headers()
install_source()

cet_script( ALWAYS_COPY updateMu2e.sh Setup_DTC.sh JAConfig.sh)

add_subdirectory(test)

# Add subdirectories for each language if desired

if (${BUILD_PYTHON_INTERFACE})
   message(-- mu2e_pcie_utils/dtcInterfaceLib::BUILD_PYTHON_INTERFACE=${BUILD_PYTHON_INTERFACE})

   find_package(SWIG REQUIRED)
   include(${SWIG_USE_FILE})
   add_subdirectory(python)
endif()

if(${BUILD_ROOT_INTERFACE})
  message(-- mu2e_pcie_utils/dtcInterfaceLib::BUILD_ROOT_INTERFACE=${BUILD_ROOT_INTERFACE})
  add_subdirectory(root)
endif()
