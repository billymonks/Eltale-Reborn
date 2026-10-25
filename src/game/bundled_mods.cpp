#include "zelda_config.h"
#include "zelda_support.h"
#include "librecomp/mods.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_set>

namespace {
    bool is_shipped_mod(const std::filesystem::path& path) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            return false;
        }
        std::string ext = path.extension().string();
        for (char& c : ext) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return ext == ".nrm" || ext == ".rtz";
    }

    std::filesystem::path bundled_mods_directory() {
        const std::filesystem::path dir = zelda64::get_executable_directory() / "mods";
        std::error_code ec;
        if (dir.empty() || !std::filesystem::is_directory(dir, ec)) {
            return {};
        }
        return dir;
    }

    std::unordered_set<std::string> mod_filenames_in(const std::filesystem::path& dir) {
        std::unordered_set<std::string> names;
        std::error_code ec;
        if (dir.empty() || !std::filesystem::is_directory(dir, ec)) {
            return names;
        }
        for (const auto& entry : std::filesystem::directory_iterator{dir, std::filesystem::directory_options::skip_permission_denied, ec}) {
            if (is_shipped_mod(entry.path())) {
                names.insert(entry.path().filename().string());
            }
        }
        return names;
    }

    std::unordered_set<std::string> read_name_list(const std::filesystem::path& path) {
        std::unordered_set<std::string> names;
        std::ifstream in{path};
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                names.insert(std::move(line));
            }
        }
        return names;
    }

    bool same_directory(const std::filesystem::path& a, const std::filesystem::path& b) {
        std::error_code ec;
        return !a.empty() && !b.empty() && std::filesystem::equivalent(a, b, ec) && !ec;
    }
}

void zelda64::install_bundled_mods() {
    const std::filesystem::path bundled = bundled_mods_directory();
    const std::filesystem::path app = zelda64::get_app_folder_path();
    const std::filesystem::path app_mods = app.empty() ? std::filesystem::path{} : app / "mods";

    // Older builds copied shipped mods into the app folder and kept loading
    // that copy after the file next to the executable was deleted. Those copies
    // are removed. The scan reads the executable's mods folder itself.
    if (!app_mods.empty() && !same_directory(bundled, app_mods)) {
        const std::filesystem::path stamp = app / "bundled_mods.txt";
        std::unordered_set<std::string> remove_names = read_name_list(stamp);
        const std::unordered_set<std::string> current = mod_filenames_in(bundled);
        remove_names.insert(current.begin(), current.end());

        std::error_code ec;
        for (const auto& name : remove_names) {
            ec.clear();
            std::filesystem::remove(app_mods / name, ec);
        }

        if (!bundled.empty()) {
            std::ofstream out{stamp, std::ios::trunc};
            for (const auto& name : current) {
                out << name << '\n';
            }
        } else {
            ec.clear();
            std::filesystem::remove(stamp, ec);
        }
    }

    if (!bundled.empty()) {
        recomp::mods::add_mod_scan_path(bundled);
    }
}
