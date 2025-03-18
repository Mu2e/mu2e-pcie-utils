
include(ArtDictionary)

art_dictionary(
  DICTIONARY_LIBRARIES
  artdaq-core-mu2e::Overlays
  mu2e-pcie-utils::CFOInterface
)

get_target_property(DictCompileOptions cfoInterfaceLib_root_dict COMPILE_OPTIONS)
if(DictCompileOptions)
	string(REPLACE "-fsanitize=thread"  "" DictCompileOptions "${DictCompileOptions}")
	string(REPLACE "-fsanitize=address" "" DictCompileOptions "${DictCompileOptions}")
	string(REPLACE "-ltsan"             "" DictCompileOptions "${DictCompileOptions}")
	string(REPLACE "-lasan"             "" DictCompileOptions "${DictCompileOptions}")

	set_target_properties(cfoInterfaceLib_root_dict PROPERTIES COMPILE_OPTIONS "${DictCompileOptions}")
endif()

