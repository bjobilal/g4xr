#define MINIZ_IMPLEMENTATION
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES  
#include "miniz.h"
#include "G4XrViewer.hh"
#include "G4VSceneHandler.hh"
#include "G4XrSceneHandler.hh" 

// tinygltf
// Define these only in *one* .cc file.
//BEN - MOVED THE COMMENTED TO XRSCENEHANDLER
//#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

namespace fs = std::filesystem;

G4XrViewer::G4XrViewer(G4VSceneHandler& sceneHandler, const G4String& name)
  : G4VViewer(sceneHandler, sceneHandler.IncrementViewCount(), name),
    fSceneHandler(sceneHandler),
    fSessionName("")
{
    fVP.SetAutoRefresh(true);
    fDefaultVP.SetAutoRefresh(true);

    std::string nameStr = name;
    auto spacePos = nameStr.find(' ');
    if (spacePos != std::string::npos) {
        fSessionName = nameStr.substr(spacePos + 1);
        fSessionName.erase(0, fSessionName.find_first_not_of(" \t"));
        fSessionName.erase(fSessionName.find_last_not_of(" \t") + 1);
        G4cout << "G4XrViewer: session name set to \"" 
               << fSessionName << "\". Will save on exit." << G4endl;
    }
}

void G4XrViewer::Initialise()
{
    std::cout << "G4XrViewer::Initialise()" << std::endl;
    server_init();
}

G4XrViewer::~G4XrViewer()
{
    svr.stop();
    if (!fSessionName.empty()) {      
        G4cout << "G4XrViewer: packaging session..." << G4endl;
        SaveSession();
    }
}

void G4XrViewer::SetView()
{
}

void G4XrViewer::DrawView()
{
    NeedKernelVisit();
    
    ProcessView();
    
    FinishView();
    
    push_file();
    
    std::cout<<"End of G4XrViewer::DrawView()"<<std::endl;
}

void G4XrViewer::ShowView() {
}

void G4XrViewer::ClearView()
{
}

void G4XrViewer::FinishView()
{
}

int G4XrViewer::server_init() // this is adapted server code that was previously used to test G4VR's web requesting functionality. Some elements are not necessary for a basic local server and can be removed for conciseness. - BEN
{
    if (fs::exists(UPLOAD_DIR)) { // cleaning code
            fs::remove_all(UPLOAD_DIR);
        }
    
    fs::create_directories(UPLOAD_DIR);

    svr.Post(R"(/upload/(\w+))", [&](const httplib::Request& req, httplib::Response& res)
    {
        std::string userId = req.matches[1];
        const auto& files = req.files;

        for (auto& [key, file] : files) {

            fs::path user_path = fs::path(UPLOAD_DIR) / userId;
            fs::create_directories(user_path);

            fs::path file_path = user_path / file.filename;
            std::ofstream ofs(file_path, std::ios::binary);
            ofs.write(file.content.data(), file.content.size());
        }

        res.set_content("Upload Complete", "text/plain");
    });

    svr.Get(R"(/files/(\w+)/list)", [&](const httplib::Request& req, httplib::Response& res)
    {
        std::string userId = req.matches[1];
        fs::path user_path = fs::path(UPLOAD_DIR) / userId;

        std::string json = "[";
        bool first = true;
        for ( auto& entry : fs::directory_iterator(user_path)) {
            if (entry.is_regular_file()) {
                if (!first) json += ",";
                json += "\"" + entry.path().filename().string() + "\"";
                first = false;
            }
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    svr.Get(R"(/files/(\w+)/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.matches[1];
        std::string filename = req.matches[2];

        fs::path file_path = fs::path(UPLOAD_DIR) / userId / filename;


        auto ifs = std::make_shared<std::ifstream>(file_path, std::ios::binary);
        
        auto file_size = fs::file_size(file_path);
        res.set_header("Content-Length", std::to_string(file_size));

        res.set_content_provider(
            "application/octet-stream",
            [file_path, file_size](size_t offset, httplib::DataSink &sink) {
                if (offset >= file_size) {
                    return false;
                }

                size_t chunk_size = 8192;
                size_t to_read = std::min(chunk_size, file_size - offset);
                std::vector<char> buffer(to_read);

                std::ifstream ifs(file_path, std::ios::binary);
                if (!ifs) return false;

                ifs.seekg(offset, std::ios::beg);
                ifs.read(buffer.data(), to_read);
                size_t bytes_read = static_cast<size_t>(ifs.gcount());

                if (bytes_read > 0) {
                    sink.write(buffer.data(), bytes_read);
                    return true;
                }

                return false;
            },
            [](bool success) {
                // no resource to clean here
            }
        );
    });

    std::string local_ip = get_local_ip();
    URL = "http://" + local_ip + ":"+std::to_string(PORT);
    G4cout << "Enter this address in G4VR: http://" << local_ip << ":" << PORT << G4endl;
    
    svr_thread = std::thread([this]() {
            svr.listen("0.0.0.0", PORT);
        });
        svr_thread.detach();

        std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}

std::string G4XrViewer::get_local_ip()
{
    std::string local_ip = "127.0.0.1";
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);

    int err = connect(sock, (const sockaddr*)&serv, sizeof(serv));
    if (err < 0)
    {
        perror("connect");
        close(sock);
        return local_ip;
    }
    sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (sockaddr*)&name, &namelen);
    if (err < 0) {
        perror("getsockname");
        close(sock);
        return local_ip;
    }
    char buffer[INET_ADDRSTRLEN];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
    if (p != nullptr) {
        local_ip = buffer;
    }

    close(sock);
    return local_ip;
}

void G4XrViewer::push_file(const std::string& dirname)
{
    httplib::Client cli(URL.c_str());
    
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path().c_str()+dirname))
    {
        if (entry.is_regular_file() && (std::find(pushedFiles.begin(), pushedFiles.end(), entry.path().filename().string()) == pushedFiles.end()))
        {
            std::cout<<"Pushing "<<entry.path().filename().string()<<" from "<<std::filesystem::current_path().c_str()+dirname<<std::endl;
            auto filepath = entry.path();
            std::ifstream ifs(filepath, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            
            httplib::MultipartFormDataItems items = {
                { "file", content, filepath.filename().string(), "application/octet-stream" }
            };
            auto res = cli.Post(("/upload/testuser"), items); // "testuser" is arbitrary as long as it is consistent with where G4VR looks...
            if (!res)
            {std::cerr << "Server Connection Lost\n"; return;}
            
            pushedFiles.push_back(entry.path().filename().string());
            
            G4UImanager::GetUIpointer()->ApplyCommand("/vis/scene/notifyHandlers");
            G4UImanager::GetUIpointer()->ApplyCommand("/vis/viewer/update");
        }

    }
}

void G4XrViewer::SaveSession()
{
    std::string zipName = fSessionName + ".zip";   

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, zipName.c_str(), 0)) 
    {
        G4cerr << "G4XrViewer: failed to create zip archive: " << zipName << G4endl;
        return;
    }

    bool anyFailed = false;
    for (const auto& entry : fs::recursive_directory_iterator(UPLOAD_DIR)) {
        if (!entry.is_regular_file()) continue;

        std::string fullPath    = entry.path().string();
        std::string archivePath = fs::relative(entry.path()).string();

        if (!mz_zip_writer_add_file(&zip, archivePath.c_str(), fullPath.c_str(),
                                     nullptr, 0, MZ_BEST_COMPRESSION)) {
            G4cerr << "G4XrViewer: failed to add to zip: " << fullPath << G4endl;
            anyFailed = true;
        }
    }

    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);

    if (!anyFailed)
        G4cout << "G4XrViewer: session saved → " << zipName << G4endl;
    else
        G4cout << "G4XrViewer: session saved with some errors → " << zipName << G4endl;

    WriteLauncherScript(zipName);
}


void G4XrViewer::WriteLauncherScript(const std::string& zipName)
{
    std::string scriptName = "g4xr_launch.py";
    std::ofstream f(scriptName);
    if (!f) {
        G4cerr << "G4XrViewer: could not write launcher script." << G4endl;
        return;
    }

    f << "#!/usr/bin/env python3\n"
         "# Auto-generated by G4XrViewer -- do not edit paths manually.\n"
         "# Run with:  python3 " << scriptName << "\n\n"

         "import http.server, zipfile, os, socket, threading\n\n"

         "ZIP  = \"" << zipName << "\"\n"
         "PORT = " << PORT << "\n"
         "UDIR = \"uploads\"\n\n"

         "def get_local_ip():\n"
         "    try:\n"
         "        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)\n"
         "        s.connect((\"8.8.8.8\", 53))\n"
         "        return s.getsockname()[0]\n"
         "    except:\n"
         "        return \"127.0.0.1\"\n\n"

         "# Unzip the session archive if uploads not already present\n"
         "if not os.path.isdir(UDIR):\n"
         "    print(f\"Extracting {ZIP} ...\")\n"
         "    with zipfile.ZipFile(ZIP, 'r') as z:\n"
         "        z.extractall(\".\")\n\n"

         "class G4XrHandler(http.server.SimpleHTTPRequestHandler):\n"
         "    \"\"\"Serve files and mimic the /files/<user>/<name> routes G4VR expects.\"\"\"\n"
         "    def log_message(self, fmt, *args):\n"
         "        pass  # silence request noise\n\n"

         "ip   = get_local_ip()\n"
         "httpd = http.server.HTTPServer((\"0.0.0.0\", PORT), G4XrHandler)\n"
         "print(f\"G4Xr session server running.\")\n"
         "print(f\"Enter this address in G4VR: http://{ip}:{PORT}\")\n"
         "print(\"Press Ctrl+C to stop.\")\n"
         "httpd.serve_forever()\n";

    f.close();
    G4cout << "G4XrViewer: launcher written → " << scriptName << G4endl;
    G4cout << "  To replay later: python3 " << scriptName << G4endl;
}


