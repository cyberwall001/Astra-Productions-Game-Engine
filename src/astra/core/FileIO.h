// core/FileIO.h - tiny filesystem helpers with atomic writes.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra::fs {

bool Exists(const std::string& path);
bool IsDirectory(const std::string& path);
bool CreateDirectories(const std::string& path);
bool RemoveFile(const std::string& path);

// Reads a whole text file; returns false when missing or unreadable.
bool ReadTextFile(const std::string& path, std::string& out);
bool ReadBinaryFile(const std::string& path, std::vector<std::uint8_t>& out);

// Writes via "<path>.tmp" + rename so a crash never leaves a half-written file.
bool WriteTextFileAtomic(const std::string& path, std::string_view contents);
bool WriteBinaryFileAtomic(const std::string& path, const void* data, std::size_t size);

std::uint64_t FileSize(const std::string& path);

// Names (not full paths) of entries in `directory`, sorted alphabetically.
std::vector<std::string> ListDirectory(const std::string& directory);

std::string AbsolutePath(const std::string& path);
std::string FileName(const std::string& path);
std::string ParentPath(const std::string& path);
std::string Extension(const std::string& path);

}  // namespace astra::fs
