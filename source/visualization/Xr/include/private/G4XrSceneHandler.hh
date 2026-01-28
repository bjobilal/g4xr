//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
//
//
//
// John Allison  5th April 2001
// A template for a simplest possible graphics driver.
//?? Lines or sections marked like this require specialisation for your driver.

#ifndef G4XRSCENEHANDLER_HH
#define G4XRSCENEHANDLER_HH

#include "G4VSceneHandler.hh"
#include "G4XrViewer.hh"

#include "G4UIManager.hh"

#include "G4Polyhedron.hh"
#include "G4Transform3D.hh"
#include "G4ThreeVector.hh"

#include <map>
#include <vector>

struct MeshData {
    std::string name;
    std::vector<G4ThreeVector> positions;
    std::vector<unsigned int> indices;
    G4Transform3D transform;
    G4Colour lvColour;
};

struct TrackData {
    std::string trackID;
    std::string particleName;
    std::string step;
    std::string x,y,z;
    std::string px,py,pz;

    std::string energy;
    
    std::string time;
    std::string edep;
    std::string process;
    
    double charge;
};

struct HitData {
    std::string x,y,z;
    std::string edep = "0.0";
};

class G4XrSceneHandler : public G4VSceneHandler
{
  public:
    G4XrSceneHandler(G4VGraphicsSystem& system, const G4String& name);
    virtual ~G4XrSceneHandler() override; // BEN - MARKED VIRTUAL

    ////////////////////////////////////////////////////////////////
    // Required implementation of pure virtual functions...

    using G4VSceneHandler::AddPrimitive;
    void AddPrimitive(const G4Polyline&) override;
    void AddPrimitive(const G4Text&) override;
    void AddPrimitive(const G4Circle&) override;
    void AddPrimitive(const G4Square&) override;
    void AddPrimitive(const G4Polyhedron&) override;
    
    void CollectTrackData(const G4VTrajectory* traj);
    void CollectHitData(const G4VHit* hit);
    void EndModeling() override;
    
    void ConvertMeshToGLB(const std::vector<MeshData>& meshList, const std::string& outputFile);
    void WriteToJSON(const std::string& filename);
    void WriteToCSV(const std::string& filename, const TrackData td);
    void WriteToCSV(const std::string& filename, const HitData hd);
    
    std::vector<MeshData> GetCollectedMeshes(){return collectedMeshes;}
    std::vector<TrackData> GetCollectedTracks(){return collectedTracks;}
    
    

  protected:
    static G4int fSceneIdCount;  // Counter for Vtk scene handlers.

  private:
    friend class G4XrViewer;
    std::vector<MeshData> collectedMeshes;
    std::vector<TrackData> collectedTracks;
    std::vector<HitData> collectedHits;
    std::unordered_set<int> loggedIDs;
    
    bool glbState = false;
    G4int runno = -1;

};

#endif
