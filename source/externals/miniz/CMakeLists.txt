add_library(G4miniz STATIC miniz.c miniz.h)

target_include_directories(G4miniz PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

set_target_properties(G4miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
