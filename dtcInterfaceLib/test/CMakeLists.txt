cet_make_exec(NAME packetGenerator SOURCE packetGenerator.cc)
cet_make_exec(NAME packetReader SOURCE packetReader.cc)

cet_make_exec(NAME dtcUnitTests SOURCE dtcUnitTests.cc LIBRARIES mu2e-pcie-utils::DTCInterface)

cet_make_exec(NAME tester SOURCE tester.cc LIBRARIES mu2e-pcie-utils::DTCInterface)

cet_make_exec(NAME sizeof_buffdesc SOURCE sizeof_buffdesc.cc LIBRARIES mu2e-pcie-utils::DTCInterface)

# Install_headers MUST BE FIRST...for some reason
install_headers()
install_source()

cet_script(ALWAYS_COPY testdata.bin)
