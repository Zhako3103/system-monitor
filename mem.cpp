#include "header.h"

static std::vector<Process> g_procs;
static TaskCounts g_counts;
static ImGuiTextFilter g_filter;
static std::set<int> g_selected;

TaskCounts getTaskCounts() { return g_counts; }

static unsigned long long readTotalJiffies() {
    std::ifstream f("/proc/stat");
    std::string cpu;
    f >> cpu;
    unsigned long long v, sum = 0;
    for (int i = 0; i < 8 && (f >> v); i++) sum += v;
    return sum;
}

static std::map<std::string, unsigned long long> readMeminfo() {
    std::map<std::string, unsigned long long> m;
    std::ifstream f("/proc/meminfo");
    std::string key, unit;
    unsigned long long val;
    while (f >> key >> val) {
        std::getline(f, unit);   // rest of line ("kB")
        if (!key.empty() && key.back() == ':') key.pop_back();
        m[key] = val;            // kB
    }
    return m;
}

static const char* stateName(char s) {
    switch (s) {
        case 'R': return "running";
        case 'S': return "sleeping";
        case 'D': return "disk sleep";
        case 'Z': return "zombie";
        case 'T': return "stopped";
        case 't': return "tracing stop";
        case 'I': return "idle";
        case 'X': return "dead";
        default:  return "unknown";
    }
}

// Re-scans /proc once per second (CPU% needs two samples to compute a delta).
void updateProcesses() {
    static double last = -10.0;
    double now = ImGui::GetTime();
    if (now - last < 1.0) return;
    last = now;

    static unsigned long long prevTotal = 0;
    static std::map<int, unsigned long long> prevProc;

    unsigned long long total = readTotalJiffies();
    unsigned long long dTotal = total > prevTotal ? total - prevTotal : 0;
    prevTotal = total;

    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1) ncpu = 1;
    long pageSize = sysconf(_SC_PAGESIZE);
    auto mi = readMeminfo();
    double memTotalBytes = (double)mi["MemTotal"] * 1024.0;

    std::vector<Process> procs;
    TaskCounts c;
    std::map<int, unsigned long long> curProc;

    DIR* d = opendir("/proc");
    if (!d) return;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        int pid = atoi(e->d_name);
        std::ifstream f(std::string("/proc/") + e->d_name + "/stat");
        if (!f) continue;
        std::string line;
        std::getline(f, line);
        size_t lp = line.find('('), rp = line.rfind(')');
        if (lp == std::string::npos || rp == std::string::npos || rp < lp) continue;

        Process p;
        p.pid = pid;
        p.name = line.substr(lp + 1, rp - lp - 1);

        std::istringstream ss(line.substr(rp + 1));
        std::vector<std::string> t;
        std::string tok;
        while (ss >> tok) t.push_back(tok);
        if (t.size() < 22) continue;   // t[0]=state(field 3) ... t[11]=utime t[12]=stime t[21]=rss

        p.state = t[0][0];
        unsigned long long proc = strtoull(t[11].c_str(), nullptr, 10) +
                                  strtoull(t[12].c_str(), nullptr, 10);
        long long rss = atoll(t[21].c_str());
        curProc[pid] = proc;

        auto it = prevProc.find(pid);
        if (it != prevProc.end() && dTotal > 0 && proc >= it->second)
            p.cpu = 100.0f * (float)ncpu * (float)(proc - it->second) / (float)dTotal;
        if (memTotalBytes > 0)
            p.mem = (float)((double)rss * (double)pageSize / memTotalBytes * 100.0);

        c.total++;
        switch (p.state) {
            case 'R': c.running++; break;
            case 'S': c.sleeping++; break;
            case 'D': c.uninterruptible++; break;
            case 'Z': c.zombie++; break;
            case 'T': case 't': c.stopped++; break;
            case 'I': c.interrupted++; break;
            default: break;
        }
        procs.push_back(p);
    }
    closedir(d);

    std::sort(procs.begin(), procs.end(),
              [](const Process& a, const Process& b) { return a.pid < b.pid; });
    g_procs.swap(procs);
    g_counts = c;
    prevProc.swap(curProc);
}

// ------------------------------------------------------------ widgets
static void usageBar(const char* title, double usedGB, double totalGB, const char* tag) {
    ImGui::Text("%s", title);
    float frac = totalGB > 0 ? (float)(usedGB / totalGB) : 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    char overlay[64];
    snprintf(overlay, sizeof(overlay), "%.2f GB/%.2f", usedGB, totalGB);
    const float reserved = 70.0f;
    ImGui::ProgressBar(frac, ImVec2(-reserved, 0.0f), overlay);
    ImGui::SameLine();
    ImGui::Text("%s", tag);

    char right[32];
    snprintf(right, sizeof(right), "%.2f GB", totalGB);
    float w = ImGui::CalcTextSize(right).x;
    ImGui::Text("0 GB");
    ImGui::SameLine(ImGui::GetWindowWidth() - reserved - w - ImGui::GetStyle().WindowPadding.x);
    ImGui::Text("%s", right);
}

void renderMemoryWindow(ImVec2 pos, ImVec2 size) {
    updateProcesses();

    auto mi = readMeminfo();
    const double kb2gb = 1.0 / (1024.0 * 1024.0);
    double ramTotal = mi["MemTotal"] * kb2gb;
    double ramUsed = ((double)mi["MemTotal"] - (double)mi["MemAvailable"]) * kb2gb;
    double swapTotal = mi["SwapTotal"] * kb2gb;
    double swapUsed = ((double)mi["SwapTotal"] - (double)mi["SwapFree"]) * kb2gb;

    double diskTotal = 0, diskUsed = 0;
    struct statvfs sv;
    if (statvfs("/", &sv) == 0) {
        diskTotal = (double)sv.f_blocks * sv.f_frsize / (1024.0 * 1024.0 * 1024.0);
        diskUsed = diskTotal - (double)sv.f_bfree * sv.f_frsize / (1024.0 * 1024.0 * 1024.0);
    }

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("== Memory and Processes ==");

    usageBar("Physic Memory (RAM) :", ramUsed, ramTotal, "RAM");
    ImGui::Spacing();
    usageBar("Virtual Memory (SWAP) :", swapUsed, swapTotal, "SWAP");
    ImGui::Spacing();
    usageBar("Disk :", diskUsed, diskTotal, "DISK");
    ImGui::Separator();

    if (ImGui::TreeNode("Process Table")) {
        ImGui::Text("filter the process by name:");
        g_filter.Draw("Filter (inc,-exc)");

        std::vector<const Process*> rows;
        for (const Process& p : g_procs)
            if (g_filter.PassFilter(p.name.c_str())) rows.push_back(&p);

        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
        float h = ImGui::GetContentRegionAvail().y - 4.0f;
        if (h < 80.0f) h = 80.0f;
        if (ImGui::BeginTable("##procs", 5, flags, ImVec2(0.0f, h))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("PID");
            ImGui::TableSetupColumn("NAME");
            ImGui::TableSetupColumn("STATE");
            ImGui::TableSetupColumn("CPU");
            ImGui::TableSetupColumn("MEM %");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin((int)rows.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const Process& p = *rows[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    char label[32];
                    snprintf(label, sizeof(label), "%d", p.pid);
                    bool sel = g_selected.count(p.pid) > 0;
                    // click toggles the row -> any number of rows can be selected
                    if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (sel) g_selected.erase(p.pid);
                        else g_selected.insert(p.pid);
                    }
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(p.name.c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(stateName(p.state));
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", p.cpu);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", p.mem);
                }
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
    ImGui::End();
}
