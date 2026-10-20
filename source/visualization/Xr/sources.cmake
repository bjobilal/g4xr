# - G4visVTK module build definition
# Define the Geant4 Module
geant4_add_module(G4visXr
  PUBLIC_HEADERS
    G4Xr.hh
  PRIVATE_HEADERS
    G4XrViewer.hh
    G4XrSceneHandler.hh
    G4XrMessenger.hh
  SOURCES
    G4Xr.cc
    G4XrViewer.cc
    G4XrSceneHandler.cc
    G4XrMessenger.cc)

geant4_module_compile_definitions(G4visXr PUBLIC G4VIS_USE_XR)

geant4_module_link_libraries(G4visXr
  PUBLIC
    G4vis_management
  PRIVATE
    G4event
    G4csg
    G4event
    G4geometrymng
    G4globman
    G4graphics_reps
    G4hepgeometry
    G4intercoms
    G4materials
    G4modeling
    G4specsolids
    G4tinygltf
    G4cpp-httplib
    G4miniz)

geant4_module_link_libraries(G4visXr PRIVATE G4UIimplementation)

