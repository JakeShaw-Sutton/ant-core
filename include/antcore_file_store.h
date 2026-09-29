#pragma once

#include <Arduino.h>
#include <FS.h>

namespace antcore_storage {

// The filesystem must support atomic rename-over-existing (LittleFS does).
// Callers serialize writes to a given path. Never unlink the committed file.
// An optional shorter temporary path must be a distinct sibling of path.
bool writeTextAtomic(fs::FS& filesystem, const char* path, const String& body,
                     const char* temporaryPath = nullptr);
bool commitReplacement(fs::FS& filesystem, const char* temporaryPath, const char* path);

}  // namespace antcore_storage
