#include "header.h"

static const double KB = 1024.0;
static const double MB = 1024.0 * 1024.0;
static const double GB = 1024.0 * 1024.0 * 1024.0;

// 452755738 -> "431.78 MB" (unit chosen so the value is never too big / too small)
std::string formatBytes(unsigned long long bytes) {
    char buf[64];
    if ((double)bytes >= GB)      snprintf(buf, sizeof(buf), "%.2f GB", bytes / GB);
    else if ((double)bytes >= MB) snprintf(buf, sizeof(buf), "%.2f MB", bytes / MB);
    else                          snprintf(buf, sizeof(buf), "%.2f KB", bytes / KB);
    return buf;
}

std::vector<NetIface> readNetDev() {
    std::vector<NetIface> res;
    std::ifstream f("/proc/net/dev");
    std::string line;
    std::getline(f, line);   // header 1
    std::getline(f, line);   // header 2
    while (std::getline(f, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        NetIface n;
        std::string name = line.substr(0, colon);
        size_t a = name.find_first_not_of(' ');
        n.name = (a == std::string::npos) ? name : name.substr(a);
        std::istringstream ss(line.substr(colon + 1));
        for (int i = 0; i < 8; i++) ss >> n.rx[i];
        for (int i = 0; i < 8; i++) ss >> n.tx[i];
        res.push_back(n);
    }
    return res;
}

std::vector<IP4> getIPv4() {
    std::vector<IP4> res;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return res;
    for (struct ifaddrs* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr, buf, sizeof(buf));
        res.push_back({p->ifa_name, buf});
    }
    freeifaddrs(ifa);
    return res;
}

static void usageBars(const std::vector<NetIface>& nets, bool rx) {
    const unsigned long long maxBytes = 2ULL * 1024 * 1024 * 1024;   // scale 0 .. 2 GB
    for (const NetIface& n : nets) {
        unsigned long long bytes = rx ? n.rx[0] : n.tx[0];
        ImGui::Text("%s:", n.name.c_str());
        float frac = (float)((double)bytes / (double)maxBytes);
        if (frac > 1.0f) frac = 1.0f;
        std::string txt = formatBytes(bytes);
        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), txt.c_str());
        ImGui::Text("0 KB");
        const char* right = "2.00 GB";
        float w = ImGui::CalcTextSize(right).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x - 10.0f);
        ImGui::TextUnformatted(right);
        ImGui::Spacing();
    }
}

void renderNetworkWindow(ImVec2 pos, ImVec2 size) {
    static std::vector<NetIface> nets;
    static std::vector<IP4> ips;
    static double last = -10.0;
    double now = ImGui::GetTime();
    if (now - last > 0.5) {
        nets = readNetDev();
        ips = getIPv4();
        last = now;
    }

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("== Network ==");

    ImGui::Text("  %s", getTimeString().c_str());
    ImGui::Spacing();
    ImGui::Text("ip4 network:");
    for (const IP4& ip : ips)
        ImGui::Text("    %s : %s", ip.name.c_str(), ip.addr.c_str());
    ImGui::Spacing();

    if (ImGui::TreeNode("network table")) {
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchSame;
        ImGui::Text("RX");
        if (ImGui::BeginTable("##rx", 9, flags)) {
            const char* cols[9] = {"interface", "bytes", "packets", "errs", "drop",
                                   "fifo", "frame", "compressed", "multicast"};
            for (int i = 0; i < 9; i++) ImGui::TableSetupColumn(cols[i]);
            ImGui::TableHeadersRow();
            for (const NetIface& n : nets) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(n.name.c_str());
                for (int i = 0; i < 8; i++) { ImGui::TableNextColumn(); ImGui::Text("%llu", n.rx[i]); }
            }
            ImGui::EndTable();
        }
        ImGui::Text("TX");
        if (ImGui::BeginTable("##tx", 9, flags)) {
            const char* cols[9] = {"interface", "bytes", "packets", "errs", "drop",
                                   "fifo", "colls", "carrier", "compressed"};
            for (int i = 0; i < 9; i++) ImGui::TableSetupColumn(cols[i]);
            ImGui::TableHeadersRow();
            for (const NetIface& n : nets) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(n.name.c_str());
                for (int i = 0; i < 8; i++) { ImGui::TableNextColumn(); ImGui::Text("%llu", n.tx[i]); }
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("##net_tabs")) {
        if (ImGui::BeginTabItem("Receive(RX)")) { usageBars(nets, true);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Receive(TX)")) { usageBars(nets, false); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
