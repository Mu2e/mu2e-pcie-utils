
add_subdirectory(basic_driver)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/basic_driver")

cet_make_exec( NAME mcs SOURCE mcs.cc LIBRARIES TRACE::TRACE)
cet_make_exec( NAME devl SOURCE devl.cc LIBRARIES TRACE::TRACE )

cet_script( ALWAYS_COPY mcs.sh )

install_headers()
install_source()
