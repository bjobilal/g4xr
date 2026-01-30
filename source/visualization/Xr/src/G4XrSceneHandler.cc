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

#include "G4XrSceneHandler.hh"

#include "G4Box.hh"
#include "G4Circle.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeModel.hh"
#include "G4Material.hh"
#include "G4Mesh.hh"
#include "G4PhysicalVolumeModel.hh"
#include "G4Polyhedron.hh"
#include "G4Polyline.hh"
#include "G4PseudoScene.hh"
#include "G4Square.hh"
#include "G4SystemOfUnits.hh"
#include "G4Text.hh"
#include "G4UnitsTable.hh"
#include "G4VNestedParameterisation.hh"
#include "G4VPhysicalVolume.hh"



#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// for track recording:

#include "G4AttHolder.hh"
#include "G4TrajectoriesModel.hh"
#include "G4HitsModel.hh"
#include "G4VTrajectory.hh"
#include "G4VTrajectoryPoint.hh"
#include "G4RichTrajectory.hh"
#include "G4RichTrajectoryPoint.hh"
#include "G4VHit.hh"
#include "G4VisAttributes.hh"

using namespace tinygltf;


// Counter for Xr scene handlers.
G4int G4XrSceneHandler::fSceneIdCount = 0;

G4XrSceneHandler::G4XrSceneHandler(G4VGraphicsSystem& system, const G4String& name)
  : G4VSceneHandler(system, fSceneIdCount++, name)
{
    // Added this to force rich trajectories
    G4UImanager::GetUIpointer()->ApplyCommand("/vis/scene/add/trajectories rich");
}

G4XrSceneHandler::~G4XrSceneHandler()
{
    fs::path gltf = fs::current_path() / "GLTF";
    fs::path uploads = fs::current_path() / "uploads";
    fs::remove_all(gltf);
    fs::remove_all(uploads);
    std::cout << "G4Xr contents deleted." << std::endl;
}

void G4XrSceneHandler::AddPrimitive(const G4Polyline& polyline)
{
    G4AttHolder holder;

    const G4VisAttributes* visAttr = polyline.GetVisAttributes();

    const G4TrajectoriesModel* trajModel = dynamic_cast<G4TrajectoriesModel*>(fpModel);
    if (trajModel)
    {
        if (trajModel->GetRunID() != runno) {runno = trajModel->GetRunID();loggedIDs.clear();} //loggedIDs is cleared as soon as a trajectory with a new run no. is seen.
        const G4VTrajectory* traj = trajModel->GetCurrentTrajectory();
        if(traj)
        {
            int trackID = traj->GetTrackID();
            if(loggedIDs.find(trackID)==loggedIDs.end()) // prevents logging a particular trajectory more than once
            {
                double r = 0.0; double g = 0.0; double b = 0.0;
                if (visAttr)
                {
                    const G4Colour c = visAttr->GetColour();
                    r = c.GetRed();
                    g = c.GetGreen();
                    b = c.GetBlue();
                }
                loggedIDs.insert(trackID);
                CollectTrackData(traj, r, g, b);
            }
        }
    }
}


void G4XrSceneHandler::AddPrimitive(const G4Text& text)
{
}


void G4XrSceneHandler::AddPrimitive(const G4Circle& circle)
{
    if (const G4HitsModel* hitsModel = dynamic_cast<G4HitsModel*>(fpModel))
    {
        const G4VHit* hit = hitsModel->GetCurrentHit();
        if (hit)
            CollectHitData(hit);

    }
}

void G4XrSceneHandler::AddPrimitive(const G4Square& square)
{
    if (const G4HitsModel* hitsModel = dynamic_cast<G4HitsModel*>(fpModel))
    {
        const G4VHit* hit = hitsModel->GetCurrentHit();
        if (hit)
            CollectHitData(hit);
    }
}

void G4XrSceneHandler::AddPrimitive(const G4Polyhedron& polyhedron)
{
    MeshData mesh;
    auto pPVModel = dynamic_cast<G4PhysicalVolumeModel*>(fpModel);

    G4String parameterisationName;
    mesh.name = "NOTPHYSVOL";
    if (pPVModel) {
        
        // model naming
        parameterisationName  = pPVModel->GetFullPVPath().back().GetPhysicalVolume()->GetName();
        mesh.name = parameterisationName;
        //G4cout<<mesh.name<<G4endl;
        
        // model colo(u)ring
        
        auto currentLV = dynamic_cast<G4PhysicalVolumeModel*>(fpModel)->GetCurrentLV();
        if (currentLV)
        {
            const G4VisAttributes* visAttr = currentLV->GetVisAttributes();
            if (visAttr)
                mesh.lvColour = visAttr->GetColour();
            else
                mesh.lvColour = G4Colour(0.5, 0.5, 0.5, 0.1);
        }
        else
        {
            mesh.lvColour = G4Colour(0.5, 0.5, 0.5, 0.1);
        }
        
        // fill in MeshData type with transform data from the G4Scene.

        mesh.transform = fObjectTransformation;
                
        int vertexno = polyhedron.GetNoVertices();
        mesh.positions.reserve(vertexno);
        for (int i = 1; i <= vertexno; ++i) {
            G4Point3D v = polyhedron.GetVertex(i);
            const G4Transform3D& T = fObjectTransformation;
            G4RotationMatrix R = T.getRotation();
            G4ThreeVector     P = T.getTranslation();
            G4ThreeVector worldV = R * v + P;
            mesh.positions.push_back(worldV);
        }
        
        int numFacets = polyhedron.GetNoFacets();
        for (int i = 1; i <= numFacets; i++) {
            G4int nEdges = 0;
            G4int nodeIndices[4];
            
            polyhedron.GetFacet(i, nEdges, nodeIndices);
            
            if (nEdges == 3) {
                mesh.indices.push_back(nodeIndices[0] - 1);
                mesh.indices.push_back(nodeIndices[1] - 1);
                mesh.indices.push_back(nodeIndices[2] - 1);
            } else if (nEdges == 4) {
                mesh.indices.push_back(nodeIndices[0] - 1);
                mesh.indices.push_back(nodeIndices[1] - 1);
                mesh.indices.push_back(nodeIndices[2] - 1);
                
                mesh.indices.push_back(nodeIndices[0] - 1);
                mesh.indices.push_back(nodeIndices[2] - 1);
                mesh.indices.push_back(nodeIndices[3] - 1);
            }
            else {std::cout<<"WARNING. A facet has neither 3 nor 4 edges"<<std::endl;}
        }
        
        collectedMeshes.push_back(std::move(mesh));
    }
    
}

auto alignTo4 = [](size_t offset) {return (offset + 3) & ~3;};

void G4XrSceneHandler::EndModeling()
{
    fs::path gltf_dir = fs::current_path() / "GLTF";
    if (!fs::exists(gltf_dir)) {fs::create_directory(gltf_dir);}

    if (!glbState)
    {
        fs::path output_path = gltf_dir / "trial.glb";
        ConvertMeshToGLB(collectedMeshes, output_path.string());
    }
}

void G4XrSceneHandler::ConvertMeshToGLB(const std::vector<MeshData>& meshList, const std::string& outputFile)
{
    tinygltf::Model model;
    tinygltf::Scene scene;
    scene.name = "G4Scene";

    for (size_t meshIndex = 0; meshIndex < meshList.size(); ++meshIndex)
    {
        const MeshData& mesh = meshList[meshIndex];
        std::vector<float> vertices;
        for ( auto& v : mesh.positions)
        {
            vertices.push_back(static_cast<float>(v.x()));
            vertices.push_back(static_cast<float>(v.y()));
            vertices.push_back(static_cast<float>(v.z()));
        }

        std::vector<uint16_t> indices(mesh.indices.begin(), mesh.indices.end());


        tinygltf::Buffer buffer;

        int vertexBufferByteLength = vertices.size() * sizeof(float);
        buffer.data.insert(buffer.data.end(),
            reinterpret_cast<const unsigned char*>(vertices.data()),
            reinterpret_cast<const unsigned char*>(vertices.data() + vertices.size()));

        int alignedVertexByteLength = alignTo4(vertexBufferByteLength);
        buffer.data.insert(buffer.data.end(), alignedVertexByteLength - vertexBufferByteLength, 0);

        int indexBufferByteOffset = alignedVertexByteLength;
        int indexBufferByteLength = indices.size() * sizeof(uint16_t);
        buffer.data.insert(buffer.data.end(),
            reinterpret_cast<const unsigned char*>(indices.data()),
            reinterpret_cast<const unsigned char*>(indices.data() + indices.size()));

        int bufferIndex = model.buffers.size();
        model.buffers.push_back(buffer);

        // position bufferViews
        tinygltf::BufferView positionBufferView;
        positionBufferView.buffer = bufferIndex;
        positionBufferView.byteOffset = 0;
        positionBufferView.byteLength = vertexBufferByteLength;
        positionBufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;

        int positionBufferViewIndex = model.bufferViews.size();
        model.bufferViews.push_back(positionBufferView);

        // index bufferViews
        tinygltf::BufferView indexBufferView;
        indexBufferView.buffer = bufferIndex;
        indexBufferView.byteOffset = indexBufferByteOffset;
        indexBufferView.byteLength = indexBufferByteLength;
        indexBufferView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

        int indexBufferViewIndex = model.bufferViews.size();
        model.bufferViews.push_back(indexBufferView);

        // position accessors
        tinygltf::Accessor positionAccessor;
        positionAccessor.bufferView = positionBufferViewIndex;
        positionAccessor.byteOffset = 0;
        positionAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        positionAccessor.count = vertices.size() / 3;
        positionAccessor.type = TINYGLTF_TYPE_VEC3;

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        float maxZ = std::numeric_limits<float>::lowest();

        for (size_t i = 0; i < vertices.size(); i += 3)
        {
            float x = vertices[i];
            float y = vertices[i + 1];
            float z = vertices[i + 2];
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            minZ = std::min(minZ, z);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            maxZ = std::max(maxZ, z);
        }

        positionAccessor.minValues = { minX, minY, minZ };
        positionAccessor.maxValues = { maxX, maxY, maxZ };

        model.accessors.push_back(positionAccessor);

        // index accessors
        tinygltf::Accessor indexAccessor;
        indexAccessor.bufferView = model.bufferViews.size() - 1;
        indexAccessor.byteOffset = 0;
        indexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        indexAccessor.count = indices.size();
        indexAccessor.type = TINYGLTF_TYPE_SCALAR;
        model.accessors.push_back(indexAccessor);
        
        // material matching
        tinygltf::Material material;
        material.name = mesh.name + "_mat";
        material.pbrMetallicRoughness.baseColorFactor = {mesh.lvColour.GetRed(), mesh.lvColour.GetGreen(), mesh.lvColour.GetBlue(), 0.1f}; // alpha specified here so G4VR doesn't have to manipulate the geometries further - BEN
        material.alphaMode = "BLEND"; // to impose opacity.
        material.pbrMetallicRoughness.metallicFactor = 0.0;
        material.pbrMetallicRoughness.roughnessFactor = 0.0; // these are defaults - no physical meaning.
        material.emissiveFactor = {mesh.lvColour.GetRed(), mesh.lvColour.GetGreen(), mesh.lvColour.GetBlue()};
        
        int materialIndex = model.materials.size();
        model.materials.push_back(material);

        tinygltf::Primitive primitive;
        primitive.attributes["POSITION"] = model.accessors.size() - 2;
        primitive.indices = model.accessors.size() - 1;
        primitive.material = materialIndex;
        primitive.mode = TINYGLTF_MODE_TRIANGLES;

        tinygltf::Mesh gltfMesh;
        gltfMesh.name = mesh.name;
        gltfMesh.primitives.push_back(primitive);
        model.meshes.push_back(gltfMesh);

        tinygltf::Node node;
        node.mesh = model.meshes.size() - 1;
        model.nodes.push_back(node);
        scene.nodes.push_back(model.nodes.size() - 1);
    }

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::TinyGLTF gltf;
    std::string err, warn;

    bool write_status = gltf.WriteGltfSceneToFile(&model, outputFile, true, true, false, true); // glb file writing
    
    if(write_status) glbState = true;

}

void G4XrSceneHandler::CollectTrackData(const G4VTrajectory* traj, G4double r ,G4double g, G4double b)
{
    G4String trackID = std::to_string(traj->GetTrackID());
    G4String particleName = traj->GetParticleName();
    G4double charge = traj->GetCharge();
    
    const G4RichTrajectory* rich_traj = dynamic_cast<const G4RichTrajectory*>(traj);

    G4int points = traj->GetPointEntries();
    for (G4int i = 0; i < points; ++i)
    {
        const G4VTrajectoryPoint* point = traj->GetPoint(i);
        if (!point) continue;

        const G4ThreeVector& pos = point->GetPosition();
        
        TrackData td;
        td.trackID = trackID;
        td.particleName = particleName;
        td.step = std::to_string(i);
        td.x = std::to_string(pos.x()); td.y = std::to_string(pos.y());td.z = std::to_string(pos.z());
        td.charge = charge;
        td.r = r; td.g = g; td.b = b;

        std::vector<G4AttValue>* attValues = point->CreateAttValues();
        
        if (rich_traj)
        {
            G4RichTrajectory* rich_traj_nc = const_cast<G4RichTrajectory*>(rich_traj);

            const G4ParticleDefinition* pdef = rich_traj_nc->GetParticleDefinition();
            G4double mass = pdef ? pdef->GetPDGMass() : 0.0;

            const G4ThreeVector initMom = rich_traj_nc->GetInitialMomentum();
            G4double p = initMom.mag();
            td.px = std::to_string(initMom.x()/CLHEP::MeV);  
            td.py = std::to_string(initMom.y()/CLHEP::MeV);
            td.pz = std::to_string(initMom.z()/CLHEP::MeV);
            G4double E = std::sqrt(p*p + mass*mass);

            td.energy = std::to_string(E/CLHEP::MeV) + " MeV";
        }

        if (attValues)
        {
            for (const auto& att : *attValues)
            {
                if (att.GetName() == "PostT") {
                    double timeNs = std::stod(att.GetValue()) / CLHEP::ns;
                    td.time = std::to_string(timeNs) + " ns";
                } else if (att.GetName() == "TED") { // total energy deposit
                    td.edep = att.GetValue();
                } else if (att.GetName() == "PDS") { // process defined step
                    td.process = att.GetValue();
                }
            }
            
            delete attValues;
        }
        collectedTracks.push_back(td);
        WriteToCSV((fs::current_path() / "GLTF" / ("run" + std::to_string(runno) + ".csv")).string(), td);
    }
}


void G4XrSceneHandler::CollectHitData(const G4VHit* hit)
{
    auto attDefs = hit->GetAttDefs();
    auto attVals = hit->CreateAttValues();

    HitData hd;

    if (attDefs && attVals)
    {
        for (size_t i = 0; i < attVals->size(); ++i)
        {
            const G4AttValue& attVal = attVals->at(i);
            const G4String& name = attVal.GetName();
            const G4String& value = attVal.GetValue();
            if (name == "Pos") {
                std::istringstream iss(value);
                G4double x, y, z;
                iss >> x >> y >> z;
                hd.x = std::to_string(x);
                hd.y = std::to_string(y);
                hd.z = std::to_string(z);
            }
            if (name == "Edep") {
                hd.edep = value;
            }
        }
        if (!hd.x.empty() && !hd.y.empty() && !hd.z.empty())
        {
            collectedHits.push_back(hd);
            WriteToCSV((fs::current_path() / "GLTF" / ("run" + std::to_string(runno) + ".csv")).string(), hd);
        }
    }
}


void G4XrSceneHandler::WriteToCSV(const std::string& filename, const TrackData td) // called with every traj entry
{
    std::ofstream file(filename,std::ios::app);
    file << "track,"<< td.trackID << ","<< td.particleName << "," << td.charge << ","<< td.step << ","<< td.x << ","<< td.y << ","<< td.z
    << ","<< td.time << ","<< td.edep<< "," << td.process << "," << td.px << ","<< td.py<< "," << td.pz << "," << td.energy << ","<< td.energy 
    << ","<< td.r << "," << td.g << "," << td.b << "\n";
    
    // the order is track, ID, pName, charge, step, x,y,z, time, edep, process, px, py, pz, energy, r, g, b
    file.close();
}

void G4XrSceneHandler::WriteToCSV(const std::string& filename, const HitData hd) // called with every hit entry
{
    std::ofstream file(filename,std::ios::app);
    file << "hit,"<< hd.x << ","<< hd.y << ","<< hd.z << "," << hd.edep << "\n";
    file.close();
}

