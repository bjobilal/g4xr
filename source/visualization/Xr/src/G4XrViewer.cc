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

    G4cout << "G4XrViewer constructor: name = \"" << name << "\"" << G4endl;  

    std::string nameStr = "Xr genius";//name;
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
    fs::path gltf = fs::current_path() / "GLTF";
    fs::path uploads = fs::current_path() / "uploads";
    fs::remove_all(gltf);
    fs::remove_all(uploads);
    std::cout << "G4Xr contents deleted." << std::endl;
}

void G4XrViewer::SetView()
{
}

void G4XrViewer::DrawView()
{
    NeedKernelVisit();
    
    ProcessView();
    
    FinishView();

    auto* xr = dynamic_cast<G4XrSceneHandler*>(&fSceneHandler);
    if(xr)
        xr->FinalizeBinary();
    
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

    std::string cmd = "zip -r " + zipName + " " + UPLOAD_DIR;
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        G4cerr << "G4XrViewer: zip failed with code " << ret << G4endl;
        return;
    }

    G4cout << "G4XrViewer: session saved to " << zipName << G4endl;
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

         "import http.server, zipfile, os, socket, re, json, cgi\n\n"

         "ZIP  = \"" << zipName << "\"\n"
         "PORT = " << PORT << "\n"
         "UDIR = \"" << UPLOAD_DIR << "\"\n\n"

         "def get_local_ip():\n"
         "    try:\n"
         "        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)\n"
         "        s.connect((\"8.8.8.8\", 53))\n"
         "        return s.getsockname()[0]\n"
         "    except:\n"
         "        return \"127.0.0.1\"\n\n"

         "if not os.path.isdir(UDIR):\n"
         "    print(f\"Extracting {ZIP} ...\")\n"
         "    with zipfile.ZipFile(ZIP, 'r') as z:\n"
         "        z.extractall(\".\")\n\n"

         "class G4XrHandler(http.server.BaseHTTPRequestHandler):\n"
         "    def log_message(self, fmt, *args):\n"
         "        pass\n\n"

         "    def do_POST(self):\n"
         "        m = re.fullmatch(r'/upload/(\\w+)', self.path)\n"
         "        if not m:\n"
         "            self.send_response(404); self.end_headers(); return\n"
         "        user_id = m.group(1)\n"
         "        user_path = os.path.join(UDIR, user_id)\n"
         "        os.makedirs(user_path, exist_ok=True)\n"
         "        ctype, pdict = cgi.parse_header(self.headers.get('Content-Type', ''))\n"
         "        pdict['boundary'] = pdict.get('boundary', '').encode()\n"
         "        pdict['CONTENT-LENGTH'] = int(self.headers.get('Content-Length', 0))\n"
         "        fields = cgi.parse_multipart(self.rfile, pdict)\n"
         "        for key, parts in fields.items():\n"
         "            # filename comes from Content-Disposition; key is the form field name\n"
         "            fname = key  # G4VR sends field name == filename\n"
         "            for data in parts:\n"
         "                out = os.path.join(user_path, fname)\n"
         "                with open(out, 'wb') as fp:\n"
         "                    fp.write(data if isinstance(data, bytes) else data.encode())\n"
         "        self.send_response(200)\n"
         "        self.send_header('Content-Type', 'text/plain')\n"
         "        self.end_headers()\n"
         "        self.wfile.write(b'Upload Complete')\n\n"

         "    def do_GET(self):\n"
         "        # Route 1: /files/<user>/list\n"
         "        m = re.fullmatch(r'/files/(\\w+)/list', self.path)\n"
         "        if m:\n"
         "            user_path = os.path.join(UDIR, m.group(1))\n"
         "            try:\n"
         "                names = [e for e in os.listdir(user_path)\n"
         "                         if os.path.isfile(os.path.join(user_path, e))]\n"
         "            except FileNotFoundError:\n"
         "                names = []\n"
         "            body = json.dumps(names).encode()\n"
         "            self.send_response(200)\n"
         "            self.send_header('Content-Type', 'application/json')\n"
         "            self.send_header('Content-Length', str(len(body)))\n"
         "            self.end_headers()\n"
         "            self.wfile.write(body)\n"
         "            return\n\n"

         "        # Route 2: /files/<user>/<filename>\n"
         "        m = re.fullmatch(r'/files/(\\w+)/(.+)', self.path)\n"
         "        if m:\n"
         "            file_path = os.path.join(UDIR, m.group(1), m.group(2))\n"
         "            try:\n"
         "                file_size = os.path.getsize(file_path)\n"
         "                self.send_response(200)\n"
         "                self.send_header('Content-Type', 'application/octet-stream')\n"
         "                self.send_header('Content-Length', str(file_size))\n"
         "                self.end_headers()\n"
         "                with open(file_path, 'rb') as fp:\n"
         "                    while True:\n"
         "                        chunk = fp.read(8192)\n"
         "                        if not chunk: break\n"
         "                        self.wfile.write(chunk)\n"
         "            except FileNotFoundError:\n"
         "                self.send_response(404); self.end_headers()\n"
         "            return\n\n"

         "        self.send_response(404); self.end_headers()\n\n"

         "ip = get_local_ip()\n"
         "httpd = http.server.HTTPServer((\"0.0.0.0\", PORT), G4XrHandler)\n"
         "print(f\"G4Xr session server running.\")\n"
         "print(f\"Enter this address in G4VR: http://{ip}:{PORT}\")\n"
         "print(\"Press Ctrl+C to stop.\")\n"
         "httpd.serve_forever()\n";

    f.close();
    G4cout << "G4XrViewer: launcher written → " << scriptName << G4endl;
    G4cout << "  To replay later: python3 " << scriptName << G4endl;
}


