#include "zelda_support.h"
#include <string>
#include <SDL.h>
#include "nfd.h"
#include "RmlUi/Core.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace zelda64 {
    // MARK: - Internal Helpers
    void perform_file_dialog_operation(const std::function<void(bool, const std::filesystem::path&)>& callback) {
        nfdnchar_t* native_path = nullptr;
        nfdresult_t result = NFD_OpenDialogN(&native_path, nullptr, 0, nullptr);

        bool success = (result == NFD_OKAY);
        std::filesystem::path path;

        if (success) {
            path = std::filesystem::path{native_path};
            NFD_FreePathN(native_path);
        }

        callback(success, path);
    }

    void perform_file_dialog_operation_multiple(const std::function<void(bool, const std::list<std::filesystem::path>&)>& callback) {
        const nfdpathset_t* native_paths = nullptr;
        nfdresult_t result = NFD_OpenDialogMultipleN(&native_paths, nullptr, 0, nullptr);

        bool success = (result == NFD_OKAY);
        std::list<std::filesystem::path> paths;
        nfdpathsetsize_t count = 0;

        if (success) {
            NFD_PathSet_GetCount(native_paths, &count);
            for (nfdpathsetsize_t i = 0; i < count; i++) {
                nfdnchar_t* cur_path = nullptr;
                nfdresult_t cur_result = NFD_PathSet_GetPathN(native_paths, i, &cur_path);
                if (cur_result == NFD_OKAY) {
                    paths.emplace_back(std::filesystem::path{cur_path});
                }
            }
            NFD_PathSet_Free(native_paths);
        }

        callback(success, paths);
    }

    // MARK: - Public API

    std::filesystem::path get_executable_directory() {
        char* base = SDL_GetBasePath();
        if (base != nullptr) {
            std::u8string text(reinterpret_cast<const char8_t*>(base));
            SDL_free(base);
            std::filesystem::path path{text};
            // SDL_GetBasePath ends with a separator, which leaves the filename empty.
            if (!path.empty() && !path.has_filename()) {
                path = path.parent_path();
            }
            return path;
        }
#ifdef _WIN32
        wchar_t buffer[32768];
        DWORD length = GetModuleFileNameW(nullptr, buffer, 32768);
        if (length > 0 && length < 32768) {
            return std::filesystem::path{buffer}.parent_path();
        }
#endif
        return {};
    }

    namespace {
        bool directory_has_asset_root(const std::filesystem::path& base) {
            if (base.empty()) {
                return false;
            }
            std::error_code ec;
            return std::filesystem::is_regular_file(base / "assets" / "recomp.rcss", ec);
        }

        // Release folders keep assets beside the exe. Running from the repo root
        // keeps them in the working directory. A double-click of the exe in
        // build-cmake has neither, but the repo root is the exe's parent.
        std::filesystem::path find_resource_root() {
            std::error_code ec;
            const std::filesystem::path cwd = std::filesystem::current_path(ec);
            if (!ec && directory_has_asset_root(cwd)) {
                return cwd;
            }

            const std::filesystem::path exe_dir = get_executable_directory();
            if (directory_has_asset_root(exe_dir)) {
                return exe_dir;
            }
            if (!exe_dir.empty() && directory_has_asset_root(exe_dir.parent_path())) {
                return exe_dir.parent_path();
            }
            return {};
        }
    }

    std::filesystem::path get_program_path() {
#if defined(__APPLE__)
        return get_bundle_resource_directory();
#elif defined(__linux__) && defined(RECOMP_FLATPAK)
        return "/app/bin";
#else
        static const std::filesystem::path root = find_resource_root();
        return root;
#endif
    }

    std::filesystem::path get_asset_path(const char* asset) {
        return get_program_path() / "assets" / asset;
    }

    void open_file_dialog(std::function<void(bool success, const std::filesystem::path& path)> callback) {
#ifdef __APPLE__
        dispatch_on_ui_thread([callback]() {
            perform_file_dialog_operation(callback);
        });
#else
        perform_file_dialog_operation(callback);
#endif
    }

    void open_file_dialog_multiple(std::function<void(bool success, const std::list<std::filesystem::path>& paths)> callback) {
#ifdef __APPLE__
        dispatch_on_ui_thread([callback]() {
            perform_file_dialog_operation_multiple(callback);
        });
#else
        perform_file_dialog_operation_multiple(callback);
#endif
    }

    void show_error_message_box(const char *title, const char *message) {
#ifdef __APPLE__
    std::string title_copy(title);
    std::string message_copy(message);

    dispatch_on_ui_thread([title_copy, message_copy] {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title_copy.c_str(), message_copy.c_str(), nullptr);
    });
#else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, nullptr);
#endif
    }
}
