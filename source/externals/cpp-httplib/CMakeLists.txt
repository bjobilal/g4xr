add_library(G4cpp-httplib INTERFACE)

target_include_directories(G4cpp-httplib INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)
if(WIN32)
  target_link_libraries(G4cpp-httplib INTERFACE ws2_32)
endif()

install(TARGETS G4cpp-httplib EXPORT Geant4LibraryDepends)
