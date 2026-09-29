#include "antcore_file_store.h"
#include <cstring>

namespace antcore_storage {

bool commitReplacement(fs::FS& filesystem, const char* temporaryPath, const char* path) {
  // LittleFS rename is the single commit point, including across power loss.
  // https://github.com/littlefs-project/littlefs#usage
  if (filesystem.rename(temporaryPath, path)) return true;
  filesystem.remove(temporaryPath);
  return false;
}

bool writeTextAtomic(fs::FS& filesystem, const char* path, const String& body, const char* temporaryPath) {
  if (path == nullptr || path[0] == '\0') return false;
  if (temporaryPath != nullptr) {
    const char* pathLeaf = strrchr(path, '/');
    const char* temporaryLeaf = strrchr(temporaryPath, '/');
    if (pathLeaf == nullptr || temporaryLeaf == nullptr || temporaryLeaf[1] == '\0' ||
        !strcmp(path, temporaryPath) || !strcmp(temporaryLeaf + 1, ".") || !strcmp(temporaryLeaf + 1, "..") ||
        pathLeaf - path != temporaryLeaf - temporaryPath ||
        strncmp(path, temporaryPath, static_cast<size_t>(pathLeaf - path + 1)) != 0) return false;
  }
  const String temporary = temporaryPath == nullptr ? String(path) + ".tmp" : String(temporaryPath);
  if (temporary.length() == 0) return false;
  filesystem.remove(temporary);
  File file = filesystem.open(temporary, FILE_WRITE);
  if (!file) return false;
  const size_t written = file.print(body);
  file.flush();
  const bool complete = written == body.length() && file.size() == body.length();
  file.close();
  if (!complete) {
    filesystem.remove(temporary);
    return false;
  }
  return commitReplacement(filesystem, temporary.c_str(), path);
}

}  // namespace antcore_storage
