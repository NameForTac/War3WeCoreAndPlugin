#include "wc3_manager.h"

#include <windows.h>
#include <cstring>
#include <algorithm>
#include <cstdio>

// ============================================================
// Implementation
// ============================================================

Wc3Manager::Wc3Manager() = default;

Wc3Manager::~Wc3Manager() {
    close();
    free_stormlib();
}

Wc3Manager::Wc3Manager(Wc3Manager&& other) noexcept
    : initialized_(other.initialized_)
    , data_dir_(std::move(other.data_dir_))
    , war3_mpq_(other.war3_mpq_)
    , war3x_mpq_(other.war3x_mpq_)
    , storm_lib_(other.storm_lib_)
    , SFileOpenArchive_(other.SFileOpenArchive_)
    , SFileOpenFileEx_(other.SFileOpenFileEx_)
    , SFileGetFileSize_(other.SFileGetFileSize_)
    , SFileReadFile_(other.SFileReadFile_)
    , SFileCloseArchive_(other.SFileCloseArchive_)
    , SFileCloseFile_(other.SFileCloseFile_)
{
    other.initialized_ = false;
    other.war3_mpq_ = nullptr;
    other.war3x_mpq_ = nullptr;
    other.storm_lib_ = nullptr;
    other.SFileOpenArchive_ = nullptr;
    other.SFileOpenFileEx_ = nullptr;
    other.SFileGetFileSize_ = nullptr;
    other.SFileReadFile_ = nullptr;
    other.SFileCloseArchive_ = nullptr;
    other.SFileCloseFile_ = nullptr;
}

Wc3Manager& Wc3Manager::operator=(Wc3Manager&& other) noexcept {
    if (this != &other) {
        close();
        free_stormlib();
        initialized_ = other.initialized_;
        data_dir_ = std::move(other.data_dir_);
        war3_mpq_ = other.war3_mpq_;
        war3x_mpq_ = other.war3x_mpq_;
        storm_lib_ = other.storm_lib_;
        SFileOpenArchive_ = other.SFileOpenArchive_;
        SFileOpenFileEx_ = other.SFileOpenFileEx_;
        SFileGetFileSize_ = other.SFileGetFileSize_;
        SFileReadFile_ = other.SFileReadFile_;
        SFileCloseArchive_ = other.SFileCloseArchive_;
        SFileCloseFile_ = other.SFileCloseFile_;
        other.initialized_ = false;
        other.war3_mpq_ = nullptr;
        other.war3x_mpq_ = nullptr;
        other.storm_lib_ = nullptr;
        other.SFileOpenArchive_ = nullptr;
        other.SFileOpenFileEx_ = nullptr;
        other.SFileGetFileSize_ = nullptr;
        other.SFileReadFile_ = nullptr;
        other.SFileCloseArchive_ = nullptr;
        other.SFileCloseFile_ = nullptr;
    }
    return *this;
}

bool Wc3Manager::load_stormlib() {
    if (storm_lib_)
        return true;
    storm_lib_ = (void*)LoadLibraryW(L"libStormLib.dll");
    if (!storm_lib_) {
        fprintf(stderr, "[Wc3Manager] LoadLibrary(libStormLib.dll) failed: err=%u\n",
                (unsigned)GetLastError());
        return false;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    SFileOpenArchive_  = (bool (*)(const char*, uint32_t, uint32_t, void**))GetProcAddress((HMODULE)storm_lib_, "SFileOpenArchive");
    SFileOpenFileEx_   = (bool (*)(void*, const char*, uint32_t, void**))      GetProcAddress((HMODULE)storm_lib_, "SFileOpenFileEx");
    SFileGetFileSize_  = (uint32_t (*)(void*, uint32_t*))                      GetProcAddress((HMODULE)storm_lib_, "SFileGetFileSize");
    SFileReadFile_     = (bool (*)(void*, void*, uint32_t, uint32_t*, void*))  GetProcAddress((HMODULE)storm_lib_, "SFileReadFile");
    SFileCloseArchive_ = (bool (*)(void*))                                      GetProcAddress((HMODULE)storm_lib_, "SFileCloseArchive");
    SFileCloseFile_    = (bool (*)(void*))                                      GetProcAddress((HMODULE)storm_lib_, "SFileCloseFile");
#pragma GCC diagnostic pop
    bool ok = (SFileOpenArchive_ && SFileOpenFileEx_ && SFileGetFileSize_ &&
               SFileReadFile_ && SFileCloseArchive_ && SFileCloseFile_);
    if (!ok) {
        fprintf(stderr, "[Wc3Manager] GetProcAddress failed: OA=%p OFE=%p GFS=%p RF=%p CA=%p CF=%p\n",
                (void*)SFileOpenArchive_, (void*)SFileOpenFileEx_, (void*)SFileGetFileSize_,
                (void*)SFileReadFile_, (void*)SFileCloseArchive_, (void*)SFileCloseFile_);
        free_stormlib();
    }
    return ok;
}

void Wc3Manager::free_stormlib() {
    if (storm_lib_) {
        FreeLibrary((HMODULE)storm_lib_);
        storm_lib_ = nullptr;
    }
    SFileOpenArchive_ = nullptr;
    SFileOpenFileEx_ = nullptr;
    SFileGetFileSize_ = nullptr;
    SFileReadFile_ = nullptr;
    SFileCloseArchive_ = nullptr;
    SFileCloseFile_ = nullptr;
}

bool Wc3Manager::try_open_mpq(const std::string& path, void*& out_handle) const {
    out_handle = nullptr;
    if (!SFileOpenArchive_)
        return false;
    void* h = nullptr;
    DWORD err = ERROR_SUCCESS;
    if (!SFileOpenArchive_(path.c_str(), 0, 0x100, &h)) {
        err = GetLastError();
        fprintf(stderr, "[Wc3Manager] SFileOpenArchive(%s) failed: err=%u\n",
                path.c_str(), (unsigned)err);
        return false;
    }
    out_handle = h;
    return true;
}

bool Wc3Manager::initialize(const std::string& wc3_data_dir) {
    close();

    if (wc3_data_dir.empty()) {
        initialized_ = false;
        return false;
    }

    if (!load_stormlib()) {
        initialized_ = false;
        return false;
    }

    data_dir_ = wc3_data_dir;

    // Build full paths and attempt to open each MPQ
    auto build_path = [&](const std::string& file) -> std::string {
        std::string p = data_dir_;
        if (!p.empty() && p.back() != '/' && p.back() != '\\')
            p += '/';
        p += file;
        for (auto& c : p) if (c == '/') c = '\\';
        return p;
    };

    try_open_mpq(build_path("war3.mpq"), war3_mpq_);
    try_open_mpq(build_path("War3x.mpq"), war3x_mpq_);

    initialized_ = (war3_mpq_ != nullptr || war3x_mpq_ != nullptr);
    fprintf(stderr, "[Wc3Manager] dir=%s war3=%s War3x=%s\n",
            data_dir_.c_str(),
            war3_mpq_ ? "ok" : "fail",
            war3x_mpq_ ? "ok" : "fail");
    return initialized_;
}

void Wc3Manager::close() {
    auto close_mpq = [&](void*& h) {
        if (h && SFileCloseArchive_) {
            SFileCloseArchive_(h);
        }
        h = nullptr;
    };
    close_mpq(war3_mpq_);
    close_mpq(war3x_mpq_);
    initialized_ = false;
    data_dir_.clear();
}

std::vector<uint8_t> Wc3Manager::read_file(const std::string& name) const {
    // Try War3x.mpq first, then War3.mpq
    void* archives[] = { war3x_mpq_, war3_mpq_ };
    for (auto* mpq : archives) {
        if (!mpq) continue;

        void* hFile = nullptr;
        if (!SFileOpenFileEx_(mpq, name.c_str(), 0, &hFile))
            continue;

        uint32_t file_size = SFileGetFileSize_(hFile, nullptr);
        if (file_size == 0 || file_size == 0xFFFFFFFF) {
            SFileCloseFile_(hFile);
            continue;
        }

        std::vector<uint8_t> buf(file_size);
        uint32_t read = 0;
        bool ok = SFileReadFile_(hFile, buf.data(), file_size, &read, nullptr);
        SFileCloseFile_(hFile);

        if (ok && read == file_size)
            return buf;
    }
    return {};
}

bool Wc3Manager::has_file(const std::string& name) const {
    void* archives[] = { war3x_mpq_, war3_mpq_ };
    for (auto* mpq : archives) {
        if (!mpq) continue;
        void* hFile = nullptr;
        if (SFileOpenFileEx_(mpq, name.c_str(), 0, &hFile)) {
            SFileCloseFile_(hFile);
            return true;
        }
    }
    return false;
}
