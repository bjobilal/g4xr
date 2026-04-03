#pragma once
#include "G4UImessenger.hh"
#include "G4UIcmdWithAString.hh"

class G4Xr;

class G4XrMessenger : public G4UImessenger {
public:
    G4XrMessenger(G4Xr* sys);
    ~G4XrMessenger();
    void SetNewValue(G4UIcommand*, G4String) override;

private:
    G4Xr*                 fSystem;
    G4UIdirectory*        fDir;
    G4UIcmdWithAString*   fSessionCmd;
};