#include "header.h"

// ------------------------------------------------------------ helpers
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return trim(ss.str());
}

// ------------------------------------------------------------ static info
std::string getOsName() {
    struct utsname u;
    if (uname(&u) == 0) return u.sysname;
    return "Unknown";
}

std::string getUserName() {
    const char* env = getenv("USER");
    if (env && *env) return env;
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_name : "unknown";
}

std::string getHostName() {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) { buf[255] = 0; return buf; }
    return "unknown";
}

std::string getCpuModel() {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 10, "model name") == 0) {
            size_t p = line.find(':');
            if (p != std::string::npos) return trim(line.substr(p + 1));
        }
    }
    return "unknown";
}

std::string getTimeString() {
    time_t t = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", localtime(&t));
    return buf;
}

// ------------------------------------------------------------ CPU usage
struct CpuTimes { unsigned long long v[8]; };

static CpuTimes readCpuTimes() {
    CpuTimes c;
    memset(&c, 0, sizeof(c));
    std::ifstream f("/proc/stat");
    std::string cpu;
    f >> cpu;
    for (int i = 0; i < 8; i++) f >> c.v[i];   // user nice system idle iowait irq softirq steal
    return c;
}

static unsigned long long cpuTotal(const CpuTimes& c) {
    unsigned long long s = 0;
    for (int i = 0; i < 8; i++) s += c.v[i];
    return s;
}

float getCpuUsage() {
    static CpuTimes prev = readCpuTimes();
    static float last = 0.0f;
    CpuTimes cur = readCpuTimes();
    unsigned long long dt = cpuTotal(cur) - cpuTotal(prev);
    if (dt == 0) return last;
    unsigned long long di = (cur.v[3] + cur.v[4]) - (prev.v[3] + prev.v[4]);
    last = 100.0f * (float)(dt - di) / (float)dt;
    prev = cur;
    return last;
}

// ------------------------------------------------------------ thermal
float getTemperature() {
    static std::string path;
    static bool searched = false;
    if (!searched) {
        searched = true;
        std::string fallback;
        for (int i = 0; i < 40; i++) {
            std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(i);
            std::string type = readFile(base + "/type");
            if (type.empty()) continue;
            if (fallback.empty()) fallback = base + "/temp";
            if (type.find("x86_pkg_temp") != std::string::npos ||
                type.find("cpu") != std::string::npos ||
                type.find("coretemp") != std::string::npos) {
                path = base + "/temp";
                break;
            }
        }
        if (path.empty()) path = fallback;
        if (path.empty()) {   // hwmon fallback
            for (int i = 0; i < 20 && path.empty(); i++) {
                std::string p = "/sys/class/hwmon/hwmon" + std::to_string(i) + "/temp1_input";
                if (!readFile(p).empty()) path = p;
            }
        }
    }
    if (path.empty()) return 0.0f;
    std::string s = readFile(path);
    return s.empty() ? 0.0f : (float)atof(s.c_str()) / 1000.0f;
}

// ------------------------------------------------------------ fan
static FanInfo readFanInfo() {
    FanInfo f;
    // ThinkPad (thinkpad_acpi)
    std::ifstream ibm("/proc/acpi/ibm/fan");
    if (ibm) {
        std::string line;
        while (std::getline(ibm, line)) {
            size_t p = line.find(':');
            if (p == std::string::npos) continue;
            std::string key = trim(line.substr(0, p));
            std::string val = trim(line.substr(p + 1));
            if (key == "status") { f.status = val; f.active = (val == "enabled"); }
            else if (key == "speed") f.speed = atoi(val.c_str());
            else if (key == "level") f.level = val;
        }
        f.available = true;
        return f;
    }
    // generic hwmon
    DIR* d = opendir("/sys/class/hwmon");
    if (d) {
        while (dirent* e = readdir(d)) {
            if (strncmp(e->d_name, "hwmon", 5) != 0) continue;
            for (int i = 1; i <= 4; i++) {
                std::string p = std::string("/sys/class/hwmon/") + e->d_name +
                                "/fan" + std::to_string(i) + "_input";
                std::string s = readFile(p);
                if (s.empty()) continue;
                f.speed = atoi(s.c_str());
                f.available = true;
                f.active = f.speed > 0;
                f.status = f.active ? "active" : "inactive";
                break;
            }
            if (f.available) break;
        }
        closedir(d);
    }
    // level from cooling device of type Fan
    for (int i = 0; i < 40; i++) {
        std::string base = "/sys/class/thermal/cooling_device" + std::to_string(i);
        std::string type = readFile(base + "/type");
        if (type.empty()) continue;
        if (type.find("Fan") != std::string::npos || type.find("fan") != std::string::npos) {
            std::string cur = readFile(base + "/cur_state");
            std::string mx = readFile(base + "/max_state");
            if (!cur.empty()) f.level = cur + (mx.empty() ? "" : " / " + mx);
            if (!f.available) { f.available = true; f.active = (cur != "0"); f.status = f.active ? "active" : "inactive"; }
            break;
        }
    }
    return f;
}

FanInfo getFanInfo() {
    static FanInfo cache;
    static double last = -10.0;
    double now = ImGui::GetTime();
    if (now - last > 0.5) { cache = readFanInfo(); last = now; }
    return cache;
}

// ------------------------------------------------------------ graph widget
static float sampleCpu()  { return getCpuUsage(); }
static float sampleTemp() { return getTemperature(); }
static float sampleFan()  { return (float)getFanInfo().speed; }

static void updateGraph(GraphState& g, float (*sampler)()) {
    if (!g.animate) return;
    double now = ImGui::GetTime();
    if (g.lastSample >= 0.0 && now - g.lastSample < 1.0 / g.fps) return;
    g.lastSample = now;
    g.last = sampler();
    g.values[g.offset] = g.last;
    g.offset = (g.offset + 1) % (int)g.values.size();
}

static void drawGraph(GraphState& g, const char* id, const char* label,
                      const char* unit, float scaleLimit) {
    ImGui::PushID(id);
    ImGui::Checkbox("Animate", &g.animate);
    ImGui::SliderFloat("FPS", &g.fps, 1.0f, 60.0f, "%.0f");
    ImGui::SliderFloat("scale max", &g.yMax, 1.0f, scaleLimit, "%.1f");
    char overlay[96];
    snprintf(overlay, sizeof(overlay), "%s : %.2f %s", label, g.last, unit);
    float h = ImGui::GetContentRegionAvail().y - 6.0f;
    if (h < 100.0f) h = 100.0f;
    ImGui::PlotLines("##plot", g.values.data(), (int)g.values.size(), g.offset,
                     overlay, 0.0f, g.yMax, ImVec2(ImGui::GetContentRegionAvail().x - 4.0f, h));
    ImGui::PopID();
}

// ------------------------------------------------------------ window
void renderSystemWindow(ImVec2 pos, ImVec2 size) {
    static GraphState gCpu(100.0f), gFan(6000.0f), gTemp(100.0f);
    static const std::string os = getOsName();
    static const std::string user = getUserName();
    static const std::string host = getHostName();
    static const std::string cpuModel = getCpuModel();

    // keep sampling all three graphs even when their tab is hidden
    updateGraph(gCpu, sampleCpu);
    updateGraph(gFan, sampleFan);
    updateGraph(gTemp, sampleTemp);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("== System ==");

    ImGui::Text("Operating System used : %s", os.c_str());
    ImGui::Text("Computer name : %s", host.c_str());
    ImGui::Text("User logged in : %s", user.c_str());
    TaskCounts c = getTaskCounts();
    ImGui::Text("Number of working processes : %d", c.total);
    ImGui::Text("  running: %d  sleeping: %d  uninterruptible: %d  zombie: %d  stopped: %d  interrupted: %d",
                c.running, c.sleeping, c.uninterruptible, c.zombie, c.stopped, c.interrupted);
    ImGui::Text("CPU : %s", cpuModel.c_str());
    ImGui::Separator();

    if (ImGui::BeginTabBar("##system_tabs")) {
        if (ImGui::BeginTabItem("CPU")) {
            drawGraph(gCpu, "cpu", "percentage", "%", 100.0f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fan")) {
            FanInfo f = getFanInfo();
            ImGui::Text("Status : %s", f.status.c_str());
            ImGui::Text("Speed  : %d RPM", f.speed);
            ImGui::Text("Level  : %s", f.level.c_str());
            if (!f.available) ImGui::TextUnformatted("(no fan sensor found on this machine)");
            drawGraph(gFan, "fan", "speed", "RPM", 10000.0f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Thermal")) {
            drawGraph(gTemp, "thermal", "temperature", "C", 150.0f);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
