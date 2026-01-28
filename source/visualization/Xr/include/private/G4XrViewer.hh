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

#ifndef G4XRVIEWER_HH
#define G4XRVIEWER_HH


#include "httplib.h"

#include "G4VViewer.hh"
#include "G4XrSceneHandler.hh"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <stdio.h>
#include <vector>

namespace fs = std::filesystem;

class G4XrViewer : public G4VViewer
{
  public:
    G4XrViewer(G4VSceneHandler&, const G4String& name);
    void Initialise() override;
    ~G4XrViewer() override;

    void SetView() override;
    void ClearView() override;
    void DrawView() override;
    void ShowView() override;
    void FinishView() override;

  protected:
    httplib::Server svr;
    std::thread svr_thread;
    std::vector<std::string> pushedFiles;
    
    
    const std::string UPLOAD_DIR = "./uploads";
    const int PORT = 2535;
    std::string URL;
    bool safe_path(std::string& path) {return path.find("..") == std::string::npos;}
    static std::string get_local_ip();
    void push_file(const std::string& dirname = "/GLTF"); 
    int server_init();
    
    
};


#endif
