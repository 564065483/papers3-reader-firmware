#pragma once

#include "app_state.h"
#include <string>
#include <vector>

namespace papers3 {

class FileManagerService {
public:
    bool ensureDirectories();
    bool list(AppState& state, const std::string& folder);
    bool renameEntry(const std::string& path, const std::string& newName, std::string& error);
    bool removeEntry(const std::string& path, std::string& error);
    bool createFolder(const std::string& parent, const std::string& name, std::string& error);
    std::string firstFile(const std::string& folder, const std::vector<std::string>& extensions) const;

private:
    static bool safeName(const std::string& name);
};

}  // namespace papers3
