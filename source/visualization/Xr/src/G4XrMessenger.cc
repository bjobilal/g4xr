#include "G4XrMessenger.hh"
#include "G4Xr.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"

G4XrMessenger::G4XrMessenger(G4Xr* sys) : fSystem(sys)
{
    fDir = new G4UIdirectory("/Xr/");
    fDir->SetGuidance("G4Xr visualisation driver commands.");

    fSessionCmd = new G4UIcmdWithAString("/Xr/session", this);
    fSessionCmd->SetGuidance("Set the session name used when saving the XR session to a zip.");
    fSessionCmd->SetParameterName("sessionName", false);
    fSessionCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

G4XrMessenger::~G4XrMessenger()
{
    delete fSessionCmd;
    delete fDir;
}

void G4XrMessenger::SetNewValue(G4UIcommand* cmd, G4String val)
{
    if (cmd == fSessionCmd)
        fSystem->SetPendingSessionName(val);
}