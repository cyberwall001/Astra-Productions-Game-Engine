#include "astra/core/FileIO.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "astra/core/Log.h"

namespace stdfs = std::filesystem;
using namespace astra;

namespace astra::fs {

bool Exists(const std::string& path) {
    std::error_code ec;
    return stdfs::exists(stdfs::u8path(path), ec);
}

bool IsDirectory(const std::string& path) {
    std::error_code ec;
    return stdfs::is_directory(stdfs::u8path(path), ec);
}

bool CreateDirectories(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    stdfs::create_directories(stdfs::u8path(path), ec);
    return !ec;
}

bool RemoveFile(const std::string& path) {
    std::error_code ec;
    return stdfs::remove(stdfs::u8path(path), ec);
}

bool ReadTextFile(const std::string& path, std::string& out) {
    std::ifstream file(stdfs::u8path(path), std::ios::binary);
    if (!file) return false;
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    out = std::move(data);
    return true;
}

bool ReadBinaryFile(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(stdfs::u8path(path), std::ios::binary);
    if (!file) return false;
    out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return true;
}

namespace {

bool WriteAtomicImpl(const std::string& path, const void* data, std::size_t size) {
    const stdfs::path target = stdfs::u8path(path);
    std::error_code ec;
    if (target.has_parent_path()) {
        stdfs::create_directories(target.parent_path(), ec);
    }
    stdfs::path temp = target;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            log::Error("fs", "cannot open for write: " + path);
            return false;
        }
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        file.flush();
        if (!file) {
            log::Error("fs", "write failed: " + path);
            return false;
        }
    }
    std::error_code renameEc;
    stdfs::rename(temp, target, renameEc);
    if (renameEc) {
        // Some platforms refuse to overwrite; remove then retry once.
        stdfs::remove(target, ec);
        renameEc.clear();
        stdfs::rename(temp, target, renameEc);
        if (renameEc) {
            stdfs::remove(temp, ec);
            log::Error("fs", "atomic rename failed: " + path);
            return false;
        }
    }
    return true;
}

}  // namespace

bool WriteTextFileAtomic(const std::string& path, std::string_view contents) {
    return WriteAtomicImpl(path, contents.data(), contents.size());
}

bool WriteBinaryFileAtomic(const std::string& path, const void* data, std::size_t size) {
    return WriteAtomicImpl(path, data, size);
}

std::uint64_t FileSize(const std::string& path) {
    std::error_code ec;
    const auto size = stdfs::file_size(stdfs::u8path(path), ec);
    return ec ? 0 : static_cast<std::uint64_t>(size);
}

std::vector<std::string> ListDirectory(const std::string& directory) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : stdfs::directory_iterator(stdfs::u8path(directory), ec)) {
        names.push_back(entry.path().filename().u8string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string AbsolutePath(const std::string& path) {
    std::error_code ec;
    const auto abs = stdfs::absolute(stdfs::u8path(path), ec);
    return ec ? path : abs.lexically_normal().u8string();
}

std::string FileName(const std::string& path) {
    return stdfs::u8path(path).filename().u8string();
}

std::string ParentPath(const std::string& path) {
    return stdfs::u8path(path).parent_path().u8string();
}

std::string Extension(const std::string& path) {
    return stdfs::u8path(path).extension().u8string();
}

}  // namespace astra::fs
