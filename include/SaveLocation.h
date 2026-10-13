#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Startup policy owned by the UI; SaveService keeps its existing file format.
struct SaveLocation {
    std::filesystem::path directory;
    std::vector<std::string> messages;
};

SaveLocation prepareSaveLocation(const std::filesystem::path& executableDirectory,
                                 const std::filesystem::path& workingDirectory);
