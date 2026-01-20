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
    if (const G4TrajectoriesModel* trajModel = dynamic_cast<G4TrajectoriesModel*>(fpModel))
    {
        if (trajModel->GetRunID() != runno) {runno = trajModel->GetRunID();loggedIDs.clear();} //loggedIDs is cleared as soon as a trajectory with a new run no. is seen.
        const G4VTrajectory* traj = trajModel->GetCurrentTrajectory();
        if(traj)
        {
            int trackID = traj->GetTrackID();
            if(loggedIDs.find(trackID)==loggedIDs.end()) // prevents logging a particular trajectory more than once
            {
                loggedIDs.insert(trackID);
                CollectTrackData(traj);
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
            G4ThreeVector worldV = fObjectTransformation * v;
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

    model.extensionsUsed.push_back("EXT_mesh_gpu_instancing");
    model.extensionsRequired.push_back("EXT_mesh_gpu_instancing");

    const MeshData& mesh = meshList[0]; 

    std::vector<float> vertices;
    for (auto& v : mesh.positions)
    {
        vertices.push_back((float)v.x());
        vertices.push_back((float)v.y());
        vertices.push_back((float)v.z());
    }

    std::vector<uint16_t> indices(mesh.indices.begin(), mesh.indices.end());

    tinygltf::Buffer buffer;
    buffer.data.insert(buffer.data.end(),
        reinterpret_cast<const unsigned char*>(vertices.data()),
        reinterpret_cast<const unsigned char*>(vertices.data() + vertices.size()));

    int alignedVertexByteLength = alignTo4(vertices.size() * sizeof(float));
    buffer.data.insert(buffer.data.end(), alignedVertexByteLength - vertices.size() * sizeof(float), 0);

    int indexOffset = alignedVertexByteLength;
    buffer.data.insert(buffer.data.end(),
        reinterpret_cast<const unsigned char*>(indices.data()),
        reinterpret_cast<const unsigned char*>(indices.data() + indices.size()));

    int bufferIndex = model.buffers.size();
    model.buffers.push_back(buffer);

    // position view
    tinygltf::BufferView posView;
    posView.buffer = bufferIndex;
    posView.byteOffset = 0;
    posView.byteLength = vertices.size() * sizeof(float);
    posView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int posViewIdx = model.bufferViews.size();
    model.bufferViews.push_back(posView);

    // index view
    tinygltf::BufferView idxView;
    idxView.buffer = bufferIndex;
    idxView.byteOffset = indexOffset;
    idxView.byteLength = indices.size() * sizeof(uint16_t);
    idxView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    int idxViewIdx = model.bufferViews.size();
    model.bufferViews.push_back(idxView);

    // position accessor
    tinygltf::Accessor posAcc;
    posAcc.bufferView = posViewIdx;
    posAcc.byteOffset = 0;
    posAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    posAcc.count = vertices.size() / 3;
    posAcc.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(posAcc);

    // index accessor
    tinygltf::Accessor idxAcc;
    idxAcc.bufferView = idxViewIdx;
    idxAcc.byteOffset = 0;
    idxAcc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    idxAcc.count = indices.size();
    idxAcc.type = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(idxAcc);


    tinygltf::Material material;
    material.name = "instanced_mat";
    material.pbrMetallicRoughness.baseColorFactor = {1, 1, 1, 1};
    material.alphaMode = "OPAQUE";
    model.materials.push_back(material);

    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = 0; 
    prim.indices = 1;                
    prim.material = 0;

    tinygltf::Mesh gltfMesh;
    gltfMesh.name = mesh.name;
    gltfMesh.primitives.push_back(prim);
    model.meshes.push_back(gltfMesh);

    std::vector<float> translations;
    std::vector<float> rotations;
    std::vector<float> scales;

    translations.insert(translations.end(), {0,0,0,  5,0,0,  0,5,0});
    rotations.insert(rotations.end(), {0,0,0,1,  0,0,0,1,  0,0,0,1});
    scales.insert(scales.end(), {1,1,1,  1,1,1,  1,1,1});

    tinygltf::Buffer instBuffer;
    instBuffer.data.insert(instBuffer.data.end(),
        reinterpret_cast<const unsigned char*>(translations.data()),
        reinterpret_cast<const unsigned char*>(translations.data() + translations.size()));

    int rotOffset = instBuffer.data.size();
    instBuffer.data.insert(instBuffer.data.end(),
        reinterpret_cast<const unsigned char*>(rotations.data()),
        reinterpret_cast<const unsigned char*>(rotations.data() + rotations.size()));

    int scaleOffset = instBuffer.data.size();
    instBuffer.data.insert(instBuffer.data.end(),
        reinterpret_cast<const unsigned char*>(scales.data()),
        reinterpret_cast<const unsigned char*>(scales.data() + scales.size()));

    int instBufferIndex = model.buffers.size();
    model.buffers.push_back(instBuffer);

    tinygltf::BufferView transView;
    transView.buffer = instBufferIndex;
    transView.byteOffset = 0;
    transView.byteLength = translations.size() * sizeof(float);
    transView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int transViewIdx = model.bufferViews.size();
    model.bufferViews.push_back(transView);

    tinygltf::BufferView rotView;
    rotView.buffer = instBufferIndex;
    rotView.byteOffset = rotOffset;
    rotView.byteLength = rotations.size() * sizeof(float);
    rotView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int rotViewIdx = model.bufferViews.size();
    model.bufferViews.push_back(rotView);

    tinygltf::BufferView scaleView;
    scaleView.buffer = instBufferIndex;
    scaleView.byteOffset = scaleOffset;
    scaleView.byteLength = scales.size() * sizeof(float);
    scaleView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int scaleViewIdx = model.bufferViews.size();
    model.bufferViews.push_back(scaleView);

    tinygltf::Accessor transAcc;
    transAcc.bufferView = transViewIdx;
    transAcc.byteOffset = 0;
    transAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    transAcc.count = translations.size() / 3;
    transAcc.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(transAcc);

    tinygltf::Accessor rotAcc;
    rotAcc.bufferView = rotViewIdx;
    rotAcc.byteOffset = 0;
    rotAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    rotAcc.count = rotations.size() / 4;
    rotAcc.type = TINYGLTF_TYPE_VEC4;
    model.accessors.push_back(rotAcc);

    tinygltf::Accessor scaleAcc;
    scaleAcc.bufferView = scaleViewIdx;
    scaleAcc.byteOffset = 0;
    scaleAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    scaleAcc.count = scales.size() / 3;
    scaleAcc.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(scaleAcc);

    tinygltf::Node node;
    node.mesh = 0;

    tinygltf::Value::Object attrObj;
    attrObj["TRANSLATION"] = tinygltf::Value((int)(model.accessors.size() - 3));
    attrObj["ROTATION"]    = tinygltf::Value((int)(model.accessors.size() - 2));
    attrObj["SCALE"]       = tinygltf::Value((int)(model.accessors.size() - 1));

    tinygltf::Value::Object instObj;
    instObj["attributes"] = tinygltf::Value(attrObj);

    node.extensions["EXT_mesh_gpu_instancing"] = tinygltf::Value(instObj);

    model.nodes.push_back(node);
    scene.nodes.push_back(model.nodes.size() - 1);
    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::TinyGLTF gltf;
    gltf.WriteGltfSceneToFile(&model, outputFile, true, true, false, true);
}


void G4XrSceneHandler::CollectTrackData(const G4VTrajectory* traj)
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

        std::vector<G4AttValue>* attValues = point->CreateAttValues();
        
        if (rich_traj)
        {
            G4RichTrajectory* rich_traj_nc = const_cast<G4RichTrajectory*>(rich_traj);

            const G4ParticleDefinition* pdef = rich_traj_nc->GetParticleDefinition();
            G4double mass = pdef ? pdef->GetPDGMass() : 0.0;

            const G4ThreeVector initMom = rich_traj_nc->GetInitialMomentum();
            G4double p = initMom.mag();
            td.px = std::to_string(initMom.x()/CLHEP::MeV);  // MeV/c
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
                    //double timeNs = std::stod(att.GetValue()) / CLHEP::ps;
                    td.time = att.GetValue();
                } else if (att.GetName() == "TED") { // total energy deposit
                    td.edep = att.GetValue();
                } else if (att.GetName() == "PDS") { // process defined step
                    td.process = att.GetValue();
                }
            }
            
            delete attValues;
        }
        collectedTracks.push_back(td);
        WriteTrackHDF5((fs::current_path() / "GLTF" / ("run" + std::to_string(runno) + ".h5")).string(), td);
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
            if (name == "Pos") 
            {
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
            WriteHitHDF5((fs::current_path() / "GLTF" / ("run" + std::to_string(runno) + ".h5")).string(), hd);
        }
    }
}

// HDF5 File Writing

void G4XrSceneHandler::InitHDF5(const std::string& fname) {
    H5::H5File file(fname, H5F_ACC_TRUNC);

    hsize_t dims[1]     = {0};
    hsize_t maxdims[1]  = {H5S_UNLIMITED};
    H5::DataSpace space(1, dims, maxdims);

    H5::DSetCreatPropList plist;
    hsize_t chunk[1] = {1024};
    plist.setChunk(1, chunk);
    plist.setDeflate(6); 

    file.createDataSet("/tracks", CreateTrack(), space, plist);
    file.createDataSet("/hits",   CreateHit(),   space, plist);
}

H5::CompType G4XrSceneHandler::CreateTrack() 
{
    H5::CompType t(sizeof(TrackData));
    t.insertMember("trackID",        HOFFSET(TrackData, trackID), H5::PredType::NATIVE_INT);
    t.insertMember("charge",         HOFFSET(TrackData, charge),  H5::PredType::NATIVE_INT);
    t.insertMember("step",           HOFFSET(TrackData, step),    H5::PredType::NATIVE_INT);
    t.insertMember("x",              HOFFSET(TrackData, x),       H5::PredType::NATIVE_DOUBLE);
    t.insertMember("y",              HOFFSET(TrackData, y),       H5::PredType::NATIVE_DOUBLE);
    t.insertMember("z",              HOFFSET(TrackData, z),       H5::PredType::NATIVE_DOUBLE);
    t.insertMember("time",           HOFFSET(TrackData, time),    H5::PredType::NATIVE_DOUBLE);
    t.insertMember("edep",           HOFFSET(TrackData, edep),    H5::PredType::NATIVE_DOUBLE);
    t.insertMember("px",              HOFFSET(TrackData, px),     H5::PredType::NATIVE_DOUBLE);
    t.insertMember("py",              HOFFSET(TrackData, py),     H5::PredType::NATIVE_DOUBLE);
    t.insertMember("pz",              HOFFSET(TrackData, pz),     H5::PredType::NATIVE_DOUBLE);
    t.insertMember("energy",         HOFFSET(TrackData, energy),  H5::PredType::NATIVE_DOUBLE);

    H5::StrType s(H5::PredType::C_S1, 32);
    t.insertMember("particleName",   HOFFSET(TrackData, particleName), s);
    t.insertMember("process",        HOFFSET(TrackData, process),      s);

    return t;
}

H5::CompType G4XrSceneHandler::CreateHit() 
{
    H5::CompType t(sizeof(HitData));
    t.insertMember("x",     HOFFSET(HitData, x),     H5::PredType::NATIVE_DOUBLE);
    t.insertMember("y",     HOFFSET(HitData, y),     H5::PredType::NATIVE_DOUBLE);
    t.insertMember("z",     HOFFSET(HitData, z),     H5::PredType::NATIVE_DOUBLE);
    t.insertMember("edep",  HOFFSET(HitData, edep),  H5::PredType::NATIVE_DOUBLE);
    return t;
}

void G4XrSceneHandler::WriteTrackHDF5(const std::string& fname, const TrackData& td)
{
    if (!std::filesystem::exists(fname))
        InitHDF5(fname);

    H5::H5File file(fname, H5F_ACC_RDWR);
    H5::DataSet ds = file.openDataSet("/tracks");

    hsize_t size;
    ds.getSpace().getSimpleExtentDims(&size);

    hsize_t newSize = size + 1;
    ds.extend(&newSize);

    H5::DataSpace fspace = ds.getSpace();
    hsize_t start[1] = {size};
    hsize_t count[1] = {1};
    fspace.selectHyperslab(H5S_SELECT_SET, count, start);

    H5::DataSpace mspace(1, count);

    TrackData row{};
    row.trackID = td.trackID;
    row.charge  = td.charge;
    row.step    = td.step;
    row.x = td.x; row.y = td.y; row.z = td.z;
    row.time = td.time;
    row.edep = td.edep;
    row.px = td.px; row.py = td.py; row.pz = td.pz;
    row.energy = td.energy;

    row.particleName = td.particleName;
    row.process = td.process;

    ds.write(&row, CreateTrack(), mspace, fspace);
}

void G4XrSceneHandler::WriteHitHDF5(const std::string& fname, const HitData& hd)
{
    if (!std::filesystem::exists(fname))
        InitHDF5(fname);

    H5::H5File file(fname, H5F_ACC_RDWR);
    H5::DataSet ds = file.openDataSet("/hits");

    hsize_t size;
    ds.getSpace().getSimpleExtentDims(&size);

    hsize_t newSize = size + 1;
    ds.extend(&newSize);

    H5::DataSpace fspace = ds.getSpace();
    hsize_t start[1] = {size};
    hsize_t count[1] = {1};
    fspace.selectHyperslab(H5S_SELECT_SET, count, start);

    H5::DataSpace mspace(1, count);

    HitData row{hd.x, hd.y, hd.z, hd.edep};
    ds.write(&row, CreateHit(), mspace, fspace);
}





