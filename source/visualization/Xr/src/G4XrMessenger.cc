#include "G4XrMessenger.hh"
#include "G4Xr.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace {

namespace fs = std::filesystem;

G4String GenerateDefaultSessionName()
{
    std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
    localtime_r(&t, &tmBuf);

    std::ostringstream dateStream;
    dateStream << std::put_time(&tmBuf, "%d_%m_%y");
    const std::string prefix = "session_" + dateStream.str() + "_";

    int index = 1;
    if (fs::exists(fs::current_path())) {
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            const std::string filename = entry.path().stem().string();
            if (filename.rfind(prefix, 0) != 0) continue;

            try {
                int existing = std::stoi(filename.substr(prefix.size()));
                if (existing >= index) index = existing + 1;
            } catch (...) {}
        }
    }

    return prefix + std::to_string(index);
}

} // namespace

G4XrMessenger::G4XrMessenger(G4Xr* sys) : fSystem(sys)
{
    fDir = new G4UIdirectory("/Xr/");
    fDir->SetGuidance("G4Xr visualisation driver commands.");

    fSessionCmd = new G4UIcmdWithAString("/Xr/session", this);
    fSessionCmd->SetGuidance("Set the session name used when saving the XR session to a zip.");
    fSessionCmd->SetParameterName("sessionName", true);
    fSessionCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

G4XrMessenger::~G4XrMessenger()
{
    delete fSessionCmd;
    delete fDir;
}

void G4XrMessenger::SetNewValue(G4UIcommand* cmd, G4String val)
{
    if (cmd == fSessionCmd) {
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);
        fSystem->SetPendingSessionName(val.empty() ? GenerateDefaultSessionName() : val);
    }
}