#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <filesystem>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input_folder output.cpio" << std::endl;
        return 1;
    }

    fs::path input_dir(argv[1]);
    fs::path output_path(argv[2]);

    fs::path config_path = input_dir / ".parserconfig";
    std::ifstream config(config_path);
    if (!config) {
        std::cerr << "Error opening config file: " << config_path << std::endl;
        return 1;
    }

    std::ofstream cpio_out(output_path, std::ios::binary);
    if (!cpio_out) {
        std::cerr << "Error creating output file: " << output_path << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(config, line)) {
        std::map<std::string, std::string> entry;
        std::istringstream iss(line);
        std::string token;

        while (iss >> token) {
            size_t eq_pos = token.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = token.substr(0, eq_pos);
                std::string value = token.substr(eq_pos + 1);
                entry[key] = value;
            }
        }

        std::string path = entry["path"];
        std::string type = entry["type"];
        std::string mode_str = entry["mode"];
        unsigned long uid = std::stoul(entry["uid"]);
        unsigned long gid = std::stoul(entry["gid"]);

        mode_t permissions = static_cast<mode_t>(std::stoul(mode_str, nullptr, 8));
        mode_t file_type = 0;
        unsigned long filesize = 0;
        unsigned long nlink = 1;
        std::string target;

        if (type == "dir") {
            file_type = S_IFDIR;
            nlink = 2;
        } else if (type == "file") {
            file_type = S_IFREG;
            fs::path file_path = input_dir / path;
            if (!fs::exists(file_path)) {
                std::cerr << "File not found: " << file_path << std::endl;
                return 1;
            }
            filesize = fs::file_size(file_path);
        } else if (type == "symlink") {
            file_type = S_IFLNK;
            target = entry["target"];
            filesize = target.size();
        } else {
            std::cerr << "Unsupported entry type: " << type << std::endl;
            return 1;
        }

        mode_t mode = file_type | (permissions & 07777);

        char header[110] = {};
        std::memcpy(header, "070701", 6); // Magic

        unsigned long namesize = path.size() + 1; // +1 for null terminator

        auto write_field = [&](int offset, unsigned long value) {
            std::snprintf(header + offset, 9, "%08lX", value);
        };

        write_field(6,  0);    // ino
        write_field(14, mode); // mode
        write_field(22, uid);  // uid
        write_field(30, gid);  // gid
        write_field(38, nlink); // nlink
        write_field(46, 0);    // mtime (not stored)
        write_field(54, filesize);
        write_field(62, 0);    // devmajor
        write_field(70, 0);    // devminor
        write_field(78, 0);    // rdevmajor
        write_field(86, 0);    // rdevminor
        write_field(94, namesize);
        write_field(102, 0);   // check

        cpio_out.write(header, 110);

        cpio_out.write(path.c_str(), path.size());
        cpio_out.put('\0');

        size_t name_pad = (4 - ((110 + namesize) % 4)) % 4;
        cpio_out.write("\0\0\0", name_pad);

        if (type == "file") {
            std::ifstream file(input_dir / path, std::ios::binary);
            std::vector<char> data(filesize);
            file.read(data.data(), filesize);
            cpio_out.write(data.data(), filesize);
        } else if (type == "symlink") {
            cpio_out.write(target.c_str(), target.size());
        }

        size_t data_pad = (4 - (filesize % 4)) % 4;
        cpio_out.write("\0\0\0", data_pad);
    }

    char trailer_header[110] = {};
    std::memcpy(trailer_header, "070701", 6);
    std::string trailer_name = "TRAILER!!!";
    unsigned long trailer_namesize = trailer_name.size() + 1;

    auto write_field = [&](int offset, unsigned long value) {
        std::snprintf(trailer_header + offset, 9, "%08lx", value);
    };

    write_field(94, trailer_namesize); // namesize
    cpio_out.write(trailer_header, 110);
    cpio_out.write(trailer_name.c_str(), trailer_name.size());
    cpio_out.put('\0');

    size_t trailer_pad = (4 - ((110 + trailer_namesize) % 4)) % 4;
    cpio_out.write("\0\0\0", trailer_pad);

    return 0;
}
