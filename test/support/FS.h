#pragma once
#include <Arduino.h>
#include <map>
#include <memory>
#include <stdexcept>
#define FILE_WRITE "w"
struct PowerLoss : std::runtime_error { PowerLoss():std::runtime_error("power lost"){} };
struct Disk {
 std::map<std::string,std::string> files;
 int failAt=-1, step=0;
 bool partial=false, renameFails=false, openFails=false;
 size_t maxNameLength=255;
 bool validName(const std::string& path) const {return path.size()-path.find_last_of('/')-1<=maxNameLength;}
 void tick() { if(step++==failAt) throw PowerLoss(); }
};
class File {
 Disk* disk=nullptr; std::string path;
 public:
 File()=default;
 File(Disk& d,std::string p):disk(&d),path(p) {}
 explicit operator bool() const { return disk!=nullptr; }
 size_t print(const String& s) { disk->tick(); size_t n=disk->partial?s.length()/2:s.length(); disk->files[path]=s.value.substr(0,n);return n; }
 void flush() {disk->tick();}
 size_t size() {disk->tick();return disk->files[path].size();}
 void close() {disk->tick();}
};
namespace fs {
class FS {
 public:
 Disk disk;
 bool remove(const String& p) {disk.tick();return disk.files.erase(p.value)>0;}
 File open(const String& p,const char*) {disk.tick();if(disk.openFails || !disk.validName(p.value))return File();disk.files[p.value]="";return File(disk,p.value);}
 bool rename(const String& from,const String& to) {
  disk.tick();if(disk.renameFails || !disk.validName(to.value))return false;
  auto entry=disk.files.find(from.value);if(entry==disk.files.end())return false;
  disk.files[to.value]=entry->second;disk.files.erase(entry);
  disk.tick();return true;
 }
};
}
