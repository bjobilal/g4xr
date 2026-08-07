#include "G4Xr.hh"
#include "G4XrSceneHandler.hh"
#include "G4XrViewer.hh"
#include "G4XrMessenger.hh" 

#include "G4UIQt.hh"
#include "G4UIbatch.hh"
#include "G4UImanager.hh"

G4String G4Xr::fPendingSessionName = "";

bool safe_path(const std::string& path)
{
    return path.find("..") == std::string::npos;
}

G4Xr::G4Xr()
  : G4VGraphicsSystem("Xr", "Xr", "Web delivery of XR file", G4VGraphicsSystem::noFunctionality)
{
    fMessenger = new G4XrMessenger(this); 
}

G4VSceneHandler* G4Xr::CreateSceneHandler(const G4String& name)
{
    return new G4XrSceneHandler(*this, name);
}

G4VViewer* G4Xr::CreateViewer(G4VSceneHandler& scene, const G4String& name)
{
    return new G4XrViewer(scene, name);
}

G4bool G4Xr::IsUISessionCompatible() const
{
    return true;  
}