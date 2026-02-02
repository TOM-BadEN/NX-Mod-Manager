#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#ifdef USE_BOOST_FILESYSTEM
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace {

    std::string replace(std::string string, const std::string &from, const std::string &to) {
        if(!from.empty())
            for(size_t pos = 0; (pos = string.find(from, pos)) != std::string::npos; pos += to.size())
                string.replace(pos, from.size(), to);
        return string;
    }

    std::string toPathString(std::string string) {
        // Replace all backslashes with forward slashes on Windows
        #if defined (_WIN32)
            string = replace(string, "\\", "/");
        #endif

        // Replace all " with \"
        string = replace(string, "\"", "\\\"");

        return string;
    }

}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::printf("./libromfs-generator <LIBROMFS_PROJECT_NAME> <LIBROMFS_RESOURCE_LOCATION>");
        return 0;
    }
    std::ofstream outputFile("libromfs_resources.cpp");

    std::printf("[libromfs] Resource Folder: %s\n", argv[2]);

    outputFile << "#include <romfs/romfs.hpp>\n\n";
    outputFile << "#include <array>\n";
    outputFile << "#include <map>\n";

    outputFile << "\n\n";
    outputFile << "/* Resource definitions */\n";

    std::vector<fs::path> paths;
    std::uint64_t identifierCount = 0;
    for (const auto &entry : fs::recursive_directory_iterator(argv[2])) {
        auto& p = entry.path();
        if (!fs::is_regular_file(p)) continue;

        auto path = fs::canonical(fs::absolute(entry.path()));
        auto relativePath = fs::relative(entry.path(), fs::absolute(argv[2]));

        if (path.filename().string() == ".DS_Store") {
            std::printf("[libromfs] SKIP: %s\n", relativePath.string().c_str());
            continue ;
        }

        outputFile << "static std::array<std::uint8_t, " << fs::file_size(p) + 1 << "> " << "resource_" + std::string(argv[1]) + "_" << identifierCount << " = {\n";
        outputFile << "    ";

        std::vector<std::byte> bytes;
        bytes.resize(fs::file_size(p));

        auto file = std::fopen(entry.path().string().c_str(), "rb");
        bytes.resize(std::fread(bytes.data(), 1, fs::file_size(p), file));
        std::fclose(file);

        outputFile << std::hex << std::uppercase << std::setfill('0') << std::setw(2);
        for (std::byte byte : bytes) {
            outputFile << "0x" << static_cast<std::uint32_t>(byte) << ", ";
        }
        outputFile << std::dec << std::nouppercase << std::setfill(' ') << std::setw(0);

        outputFile << "\n 0x00 };\n\n";

        paths.push_back(relativePath);

        identifierCount++;
    }

    outputFile << "\n";

    {
        outputFile << "/* Resource map */\n";
        outputFile << "const std::map<fs::path, romfs::Resource>& RomFs_" + std::string(argv[1]) + "_get_resources() {\n";
        outputFile << "    static std::map<fs::path, romfs::Resource> resources = {\n";

        for (std::uint64_t i = 0; i < identifierCount; i++) {

            std::printf("[libromfs] Bundling resource: %s\n", paths[i].string().c_str());

            outputFile << "        " << "{ \"" << toPathString(paths[i].string()) << "\", romfs::Resource({ reinterpret_cast<std::byte*>(resource_" + std::string(argv[1]) + "_" << i << ".data()), " << "resource_" + std::string(argv[1]) + "_" << i << ".size() - 1 }) " << "},\n";
        }
        outputFile << "    };";

        outputFile << "\n\n    return resources;\n";
        outputFile << "}\n\n";
    }

    outputFile << "\n\n";

    {
        outputFile << "/* Resource paths */\n";
        outputFile << "const std::vector<fs::path>& RomFs_" + std::string(argv[1]) + "_get_paths() {\n";
        outputFile << "    static std::vector<fs::path> paths = {\n";

        for (std::uint64_t i = 0; i < identifierCount; i++) {
            outputFile << "        \"" << toPathString(paths[i].string()) << "\",\n";
        }
        outputFile << "    };";

        outputFile << "\n\n    return paths;\n";
        outputFile << "}\n\n";
    }

    outputFile << "\n\n";

    {
        outputFile << "/* RomFS name */\n";
        outputFile << "const std::string& RomFs_" + std::string(argv[1]) + "_get_name() {\n";
        outputFile << "    static std::string name = \"" + std::string(argv[1]) + "\";\n";
        outputFile << "    return name;\n";
        outputFile << "}\n\n";
    }

    outputFile << "\n\n";
}
