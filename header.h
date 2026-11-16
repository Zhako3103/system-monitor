#ifndef HEADER_H
#define HEADER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "imgui.h"

// ---------- graph (CPU / Fan / Thermal) ----------
struct GraphState {
    std::vector<float> values;
    int offset = 0;
    double lastSample = -1.0;
    float last = 0.0f;
    bool animate = true;
    float fps = 30.0f;
    float yMax = 100.0f;
    explicit GraphState(float defaultMax = 100.0f) : values(100, 0.0f), yMax(defaultMax) {}
};

struct FanInfo {
    bool available = false;
    bool active = false;
    int speed = 0;            // RPM
    std::string level = "n/a";
    std::string status = "not available";
};

// ---------- processes ----------
struct Process {
    int pid = 0;
    std::string name;
    char state = '?';
    float cpu = 0.0f;   // %
    float mem = 0.0f;   // %
};

struct TaskCounts {
    int total = 0, running = 0, sleeping = 0, uninterruptible = 0,
        zombie = 0, stopped = 0, interrupted = 0;
};

// ---------- network ----------
struct NetIface {
    std::string name;
    unsigned long long rx[8];   // bytes packets errs drop fifo frame compressed multicast
    unsigned long long tx[8];   // bytes packets errs drop fifo colls carrier compressed
};

struct IP4 {
    std::string name;
    std::string addr;
};

// ---------- system.cpp ----------
std::string readFile(const std::string& path);
std::string getOsName();
std::string getUserName();
std::string getHostName();
std::string getCpuModel();
float       getCpuUsage();
float       getTemperature();
FanInfo     getFanInfo();
std::string getTimeString();
void        renderSystemWindow(ImVec2 pos, ImVec2 size);

// ---------- mem.cpp ----------
void        updateProcesses();
TaskCounts  getTaskCounts();
void        renderMemoryWindow(ImVec2 pos, ImVec2 size);

// ---------- network.cpp ----------
std::string formatBytes(unsigned long long bytes);
std::vector<NetIface> readNetDev();
std::vector<IP4>      getIPv4();
void        renderNetworkWindow(ImVec2 pos, ImVec2 size);

#endif
