#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <system_error>
#include <iomanip>
#include <cstdio>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input.cpio output_folder" << std::endl;
        return 1;
    }

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Error opening input file: " << argv[1] << std::endl;
        return 1;
    }

    fs::create_directory(argv[2]);

    fs::path config_path = fs::path(argv[2]) / ".parserconfig";
    std::ofstream config(config_path);
    if (!config) {
        std::cerr << "Error creating config file" << std::endl;
        return 1;
    }

    while (true) {
        char header[110];
        if (!in.read(header, 110))
            break;

        std::string magic(header, 6);
        if (magic != "070701") {
            std::cerr << "Unsupported format" << std::endl;
            return 1;
        }

        auto read_field = [&](int offset) -> unsigned long {
            std::string field(header + offset, 8);
            return std::strtoul(field.c_str(), nullptr, 16);
        };

        unsigned long ino       = read_field(6);
        unsigned long mode      = read_field(14);
        unsigned long uid       = read_field(22);
        unsigned long gid       = read_field(30);
        unsigned long nlink     = read_field(38);
        unsigned long mtime     = read_field(46);
        unsigned long filesize  = read_field(54);
        unsigned long devmajor  = read_field(62);
        unsigned long devminor  = read_field(70);
        unsigned long rdevmajor = read_field(78);
        unsigned long rdevminor = read_field(86);
        unsigned long namesize  = read_field(94);
        unsigned long check     = read_field(102);

        std::vector<char> namebuf(namesize);
        in.read(namebuf.data(), namesize);
        std::string filename(namebuf.data(), namesize - 1);

        int header_and_name = 110 + namesize;
        int pad = (4 - (header_and_name % 4)) % 4;
        in.ignore(pad);

        if (filename == "TRAILER!!!")
            break;

        fs::path outpath = fs::path(argv[2]) / filename;

        fs::create_directories(outpath.parent_path());

        mode_t file_mode = mode & 07777;
        mode_t file_type = mode & S_IFMT;

        char mode_str[6];
        std::snprintf(mode_str, sizeof(mode_str), "0%03o", static_cast<unsigned int>(file_mode));

        if (file_type == S_IFDIR) {
            fs::create_directory(outpath);
            config << "path=" << filename << " type=dir mode=" << mode_str
                   << " uid=" << uid << " gid=" << gid << "\n";
        } else if (file_type == S_IFREG) {
            std::ofstream outfile(outpath, std::ios::binary);
            if (!outfile) {
                std::cerr << "Error creating file: " << outpath << std::endl;
                return 1;
            }
            std::vector<char> filedata(filesize);
            in.read(filedata.data(), filesize);
            outfile.write(filedata.data(), filesize);
            outfile.close();
            config << "path=" << filename << " type=file mode=" << mode_str
                   << " uid=" << uid << " gid=" << gid << "\n";
        } else if (file_type == S_IFLNK) {
            std::vector<char> linkdata(filesize);
            in.read(linkdata.data(), filesize);
            std::string target(linkdata.data(), filesize);
            config << "path=" << filename << " type=symlink mode=" << mode_str
                   << " uid=" << uid << " gid=" << gid << " target=" << target << "\n";
        } else {
            std::cerr << "Unsupported file type" << std::endl;
            in.ignore(filesize);
        }

        int pad2 = (4 - (filesize % 4)) % 4;
        in.ignore(pad2);
    }

    return 0;
}
