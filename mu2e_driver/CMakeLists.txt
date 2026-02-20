if (DEFINED ENV{KDIR})
  set(KDIR "$ENV{KDIR}")
  execute_process(COMMAND cat ${KDIR}/include/config/kernel.release OUTPUT_VARIABLE KVERS OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
  execute_process(COMMAND uname -r OUTPUT_VARIABLE KVERS OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach(_variableName ${_variableNames})
#	message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

if(EXISTS "${TRACE_BIN_DIR}/../module/${KVERS}/TRACE.ko")
  # Using a UPS product version of TRACE
  set(TRACE_KO "${TRACE_BIN_DIR}/../module/${KVERS}/TRACE.ko")
  set(ESYMS "${TRACE_BIN_DIR}/../module/${KVERS}/Module.symvers")
else()
  # MRB Build or TRACE not available. WILL FAIL if not available!
  #set(TRACE_KO "${CMAKE_BINARY_DIR}/TRACE/module/${KVERS}/TRACE.ko")
  set(TRACE_KO "$ENV{TRACE_DIR}/include/trace.h")
  set(ESYMS "${CMAKE_BINARY_DIR}/TRACE/module/${KVERS}/Module.symvers")
endif()

if (${WANT_KMOD})
    if (EXISTS /lib/modules/${KVERS}/build)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/../drivers/${KVERS}/mu2e.ko
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Makefile ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Kbuild ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_event.c ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_event.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_fs.c ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_fs.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_mem.c ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_mem.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_main.c ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_mmap_ioctl.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_pci.c ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_pci.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_proto_globals.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/xdma_hw.h ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND make CC=/usr/bin/gcc EXTRA_SYMBOLS="KBUILD_EXTRA_SYMBOLS=${ESYMS}" TRACE_INC="${TRACE_INCLUDE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/../drivers/${KVERS}
            COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/mu2e.ko ${CMAKE_CURRENT_BINARY_DIR}/../drivers/${KVERS}/mu2e.ko
            COMMAND make clean
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_event.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_fs.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_mem.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_main.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_pci.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_event.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_fs.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_mem.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_mmap_ioctl.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_pci.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/mu2e_proto_globals.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/xdma_hw.h
                    ${TRACE_KO}
        )

        add_custom_target(mu2e_driver ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../drivers/${KVERS}/mu2e.ko)
        install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../drivers DESTINATION .)
    else()
        message(FATAL_ERROR "KMOD build requested, but kernel-devel package is not installed!")
    endif()
endif()

install_headers()
install_source()
