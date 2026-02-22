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

void G4XrSceneHandler::InitializeBinary()
{
    std::string path = (fs::current_path() / "GLTF" / ("run" + std::to_string(runno) + ".bin")).string();
    out.open(path,std::ios::binary);
    header = Header(); 
    out.write(header.magic, 4);
    out.write((char*)&header.version, 4);
    out.write((char*)&header.stringCount, 4);
    out.write((char*)&header.trackCount, 4);
}

void G4XrSceneHandler::FinalizeBinary()
{
    for(auto& s : stringTable)
    {
        uint16_t len = s.size();
        out.write((char*)&len,2);
        out.write(s.data(),len);
    }
    out.seekp(0);
    header.stringCount = stringTable.size();
    out.write((char*)&header,sizeof(header));
    out.close();
}

// helper function(s) for track data writing

uint16_t G4XrSceneHandler::GetStringID(const std::string& s)
{
    auto it = stringIndex.find(s);
    if(it!=stringIndex.end())
        return it->second;

    uint16_t id = stringTable.size();
    stringTable.push_back(s);
    stringIndex[s] = id;
    return id;
}


void G4XrSceneHandler::AddPrimitive(const G4Polyline& polyline)
{
    G4AttHolder holder;

    const G4VisAttributes* visAttr = polyline.GetVisAttributes();

    const G4TrajectoriesModel* trajModel = dynamic_cast<G4TrajectoriesModel*>(fpModel);
    if (trajModel)
    {
        if (trajModel->GetRunID() != runno) 
        {
            runno = trajModel->GetRunID();
            loggedIDs.clear(); //loggedIDs is cleared as soon as a trajectory with a new run no. is seen.
            InitializeBinary(); // create a new binary file.
        }
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
    auto pPVModel = dynamic_cast<G4PhysicalVolumeModel*>(fpModel);
    if (!pPVModel) return;

    std::string meshName =
        pPVModel->GetFullPVPath().back().GetPhysicalVolume()->GetName();

    G4Colour colour(0.5, 0.5, 0.5, 0.1);
    auto currentLV = pPVModel->GetCurrentLV();
    if (currentLV) {
        const G4VisAttributes* visAttr = currentLV->GetVisAttributes();
        if (visAttr)
            colour = visAttr->GetColour();
    }

    int uniqueIndex;

    if (meshMap.find(meshName) == meshMap.end())
    {
        MeshData mesh;
        mesh.name = meshName;
        mesh.lvColour = colour;

        int vertexno = polyhedron.GetNoVertices();
        mesh.positions.reserve(vertexno);

        for (int i = 1; i <= vertexno; ++i) {
            G4Point3D v = polyhedron.GetVertex(i);
            mesh.positions.push_back(v);  
        }

        int numFacets = polyhedron.GetNoFacets();
        for (int i = 1; i <= numFacets; i++) {
            G4int nEdges = 0;
            G4int nodeIndices[4];

            polyhedron.GetFacet(i, nEdges, nodeIndices);

            if (nEdges == 3) {
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[0] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[1] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[2] - 1));
            }
            else if (nEdges == 4) {
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[0] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[1] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[2] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[0] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[2] - 1));
                    mesh.indices.push_back(static_cast<uint32_t>(nodeIndices[3] - 1));
            }
        }

        uniqueIndex = uniqueMeshes.size();
        uniqueMeshes.push_back(std::move(mesh));
        meshMap[meshName] = uniqueIndex;
    }
    else
    {
        uniqueIndex = meshMap[meshName];
    }

    InstanceData instance;
    instance.uniqueMeshIndex = uniqueIndex;
    instance.transform = fObjectTransformation;
    instance.colour = colour;

    instances.push_back(instance);
}

auto alignTo4 = [](size_t offset) {return (offset + 3) & ~3;};

void G4XrSceneHandler::EndModeling()
{
    fs::path gltf_dir = fs::current_path() / "GLTF";
    if (!fs::exists(gltf_dir)) {fs::create_directory(gltf_dir);}

    if (!glbState)
    {
        fs::path output_path = gltf_dir / "trial.glb";
        ConvertMeshToGLB(output_path.string());
    }
}

void DecomposeTransform( const G4Transform3D& T, std::vector<double>& translation, std::vector<double>& rotation, std::vector<double>& scale) {
    G4ThreeVector P = T.getTranslation();
    G4RotationMatrix R = T.getRotation();

    translation = { P.x(), P.y(), P.z() };
    scale = { 1.0, 1.0, 1.0 };

    double m00 = R.xx();
    double m01 = R.xy();
    double m02 = R.xz();
    double m10 = R.yx();
    double m11 = R.yy();
    double m12 = R.yz();
    double m20 = R.zx();
    double m21 = R.zy();
    double m22 = R.zz();

    double qw, qx, qy, qz;

    double trace = m00 + m11 + m22;

    if (trace > 0.0) {
        double s = 0.5 / std::sqrt(trace + 1.0);
        qw = 0.25 / s;
        qx = (m21 - m12) * s;
        qy = (m02 - m20) * s;
        qz = (m10 - m01) * s;
    } else {
        if (m00 > m11 && m00 > m22) {
            double s = 2.0 * std::sqrt(1.0 + m00 - m11 - m22);
            qw = (m21 - m12) / s;
            qx = 0.25 * s;
            qy = (m01 + m10) / s;
            qz = (m02 + m20) / s;
        } else if (m11 > m22) {
            double s = 2.0 * std::sqrt(1.0 + m11 - m00 - m22);
            qw = (m02 - m20) / s;
            qx = (m01 + m10) / s;
            qy = 0.25 * s;
            qz = (m12 + m21) / s;
        } else {
            double s = 2.0 * std::sqrt(1.0 + m22 - m00 - m11);
            qw = (m10 - m01) / s;
            qx = (m02 + m20) / s;
            qy = (m12 + m21) / s;
            qz = 0.25 * s;
        }
    }

    rotation = { qx, qy, qz, qw };
}

void G4XrSceneHandler::ConvertMeshToGLB(const std::string& outputFile)
{
    tinygltf::Model model;
    tinygltf::Scene scene;
    scene.name = "G4Scene";

    std::vector<int> gltfMeshIndices;

    for (const auto& mesh : uniqueMeshes)
    {
        std::vector<float> vertices;
        for (auto& v : mesh.positions)
        {
            vertices.push_back(v.x());
            vertices.push_back(v.y());
            vertices.push_back(v.z());
        }

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

        std::vector<uint16_t> indices(mesh.indices.begin(), mesh.indices.end());

        tinygltf::Buffer buffer;

        size_t vertexBytes = vertices.size() * sizeof(float);
        buffer.data.insert(buffer.data.end(),
            reinterpret_cast<unsigned char*>(vertices.data()),
            reinterpret_cast<unsigned char*>(vertices.data()) + vertexBytes);

        size_t alignedVertexBytes = alignTo4(vertexBytes);
        buffer.data.insert(buffer.data.end(),
            alignedVertexBytes - vertexBytes, 0);

        size_t indexOffset = alignedVertexBytes;
        size_t indexBytes = indices.size() * sizeof(uint16_t);

        buffer.data.insert(buffer.data.end(),
            reinterpret_cast<unsigned char*>(indices.data()),
            reinterpret_cast<unsigned char*>(indices.data()) + indexBytes);

        int bufferIndex = model.buffers.size();
        model.buffers.push_back(buffer);

        // BufferViews
        tinygltf::BufferView posView;
        posView.buffer = bufferIndex;
        posView.byteOffset = 0;
        posView.byteLength = vertexBytes;
        posView.target = TINYGLTF_TARGET_ARRAY_BUFFER;

        int posViewIndex = model.bufferViews.size();
        model.bufferViews.push_back(posView);

        tinygltf::BufferView idxView;
        idxView.buffer = bufferIndex;
        idxView.byteOffset = indexOffset;
        idxView.byteLength = indexBytes;
        idxView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

        int idxViewIndex = model.bufferViews.size();
        model.bufferViews.push_back(idxView);

        // Accessors
        tinygltf::Accessor posAccessor;
        posAccessor.bufferView = posViewIndex;
        posAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        posAccessor.count = vertices.size() / 3;
        posAccessor.type = TINYGLTF_TYPE_VEC3;
        posAccessor.minValues = { minX, minY, minZ };
        posAccessor.maxValues = { maxX, maxY, maxZ };
        int posAccessorIndex = model.accessors.size();
        model.accessors.push_back(posAccessor);

        tinygltf::Accessor idxAccessor;
        idxAccessor.bufferView = idxViewIndex;
        idxAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        idxAccessor.count = indices.size();
        idxAccessor.type = TINYGLTF_TYPE_SCALAR;

        int idxAccessorIndex = model.accessors.size();
        model.accessors.push_back(idxAccessor);

        // Material
        tinygltf::Material material;
        material.pbrMetallicRoughness.baseColorFactor = {
            mesh.lvColour.GetRed(),
            mesh.lvColour.GetGreen(),
            mesh.lvColour.GetBlue(),
            0.1f
        };
        material.alphaMode = "BLEND";

        int materialIndex = model.materials.size();
        model.materials.push_back(material);

        tinygltf::Primitive primitive;
        primitive.attributes["POSITION"] = posAccessorIndex;
        primitive.indices = idxAccessorIndex;
        primitive.material = materialIndex;
        primitive.mode = TINYGLTF_MODE_TRIANGLES;  

        tinygltf::Mesh gltfMesh;
        gltfMesh.name = mesh.name;
        gltfMesh.primitives.push_back(primitive);

        int gltfMeshIndex = model.meshes.size();
        model.meshes.push_back(gltfMesh);

        gltfMeshIndices.push_back(gltfMeshIndex);
    }

    for (const auto& inst : instances)
    {
        tinygltf::Node node;
        node.mesh = gltfMeshIndices[inst.uniqueMeshIndex];

        std::vector<double> t, r, s;
        DecomposeTransform(inst.transform, t, r, s);

        node.translation = t;
        node.rotation = r;
        node.scale = s;

        int nodeIndex = model.nodes.size();
        model.nodes.push_back(node);
        scene.nodes.push_back(nodeIndex);
    }

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::TinyGLTF gltf;
    std::string err, warn;

    gltf.WriteGltfSceneToFile(&model, outputFile, true, true, false, true);
}

void G4XrSceneHandler::CollectTrackData(const G4VTrajectory* traj, G4double r,G4double g,G4double b)
{
    TrackData t;

    t.trackID = traj->GetTrackID();
    t.particleName = traj->GetParticleName();
    t.charge = traj->GetCharge();

    t.r=r; t.g=g; t.b=b;

    const G4RichTrajectory* rich = dynamic_cast<const G4RichTrajectory*>(traj);
    G4RichTrajectory* rich_nc = const_cast<G4RichTrajectory*>(rich);

    double mass=0;
    if(rich_nc && rich_nc->GetParticleDefinition())
        mass = rich_nc->GetParticleDefinition()->GetPDGMass();

    int N = traj->GetPointEntries();

    for(int i=0;i<N;i++)
    {
        auto* p = traj->GetPoint(i);
        if(!p) continue;

        auto pos = p->GetPosition();

        t.x.push_back(pos.x());
        t.y.push_back(pos.y());
        t.z.push_back(pos.z());

        if(rich)
        {
            auto mom = rich_nc->GetInitialMomentum();
            t.px.push_back(mom.x());
            t.py.push_back(mom.y());
            t.pz.push_back(mom.z());
            double P = mom.mag();
            t.energy.push_back(std::sqrt(P*P + mass*mass));
        }

        auto* atts = p->CreateAttValues();

        float time=0,edep=0;
        uint16_t procID=0;

        if(atts)
        {
            for(auto& a:*atts)
            {
                if(a.GetName()=="PostT")
                    time = std::stod(a.GetValue());

                else if(a.GetName()=="TED")
                    edep = std::stod(a.GetValue());

                else if(a.GetName()=="PDS")
                    procID = GetStringID(a.GetValue());
            }
            delete atts;
        }

        t.time.push_back(time);
        t.edep.push_back(edep);
        t.processID.push_back(procID);
    }
    WriteTrackBinary(t);
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
            //WriteToCSV((fs::current_path() / "GLTF" / ("run" + std::to_string(runno) + ".csv")).string(), hd);
        }
    }
}

void G4XrSceneHandler::WriteTrackBinary(const TrackData& t)
{
    std::streampos writePos = out.tellp(); // remember where we are

    header.trackCount++;

    uint32_t n = t.x.size();
    uint16_t nameID = GetStringID(t.particleName);

    out.write((char*)&t.trackID,4);
    out.write((char*)&nameID,2);
    out.write((char*)&t.charge,8);

    out.write((char*)&t.r,sizeof(float));
    out.write((char*)&t.g,sizeof(float));
    out.write((char*)&t.b,sizeof(float));

    out.write((char*)&n,4);

    out.write((char*)t.x.data(),sizeof(float)*n);
    out.write((char*)t.y.data(),sizeof(float)*n);
    out.write((char*)t.z.data(),sizeof(float)*n);

    out.write((char*)t.time.data(),sizeof(float)*n);
    out.write((char*)t.edep.data(),sizeof(float)*n);

    out.write((char*)t.px.data(),sizeof(float)*n);
    out.write((char*)t.py.data(),sizeof(float)*n);
    out.write((char*)t.pz.data(),sizeof(float)*n);
    out.write((char*)t.energy.data(),sizeof(float)*n);

    out.write((char*)t.processID.data(),sizeof(uint16_t)*n);

    // --- update header in file ---
    std::streampos endPos = out.tellp();
    out.seekp(0);

    header.stringCount = stringTable.size();

    out.write(header.magic,4);
    out.write((char*)&header.version,4);
    out.write((char*)&header.stringCount,4);
    out.write((char*)&header.trackCount,4);

    out.seekp(endPos); // return to end for next write
    out.flush();
}


void G4XrSceneHandler::WriteToCSV(const std::string& filename, const HitData hd) // called with every hit entry
{
    std::ofstream file(filename,std::ios::app);
    file << "hit,"<< hd.x << ","<< hd.y << ","<< hd.z << "," << hd.edep << "\n";
    file.close();
}

