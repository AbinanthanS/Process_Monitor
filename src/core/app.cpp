#include "core/app.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <csignal>
#include <sys/types.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGCONT
#define SIGCONT 18
#endif
#ifndef SIGSTOP
#define SIGSTOP 19
#endif

static int sendProcessSignal(int pid, int signalNum) {
    if (signalNum == SIGKILL || signalNum == SIGTERM) {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (hProc) {
            BOOL res = TerminateProcess(hProc, 1);
            CloseHandle(hProc);
            return res ? 0 : -1;
        }
    }
    return -1;
}
#else
#include <unistd.h>
static int sendProcessSignal(int pid, int signalNum) {
    return kill(pid, signalNum);
}
#endif

using namespace std;

App::App() : term(Terminal::instance()) {
    term.init();
    collectorThread = thread(&App::collectorLoop, this);
}

App::~App() {
    running.store(false);
    if (collectorThread.joinable()) {
        collectorThread.join();
    }
    term.cleanup();
}

void App::setStatus(const string& msg, int durationSeconds) {
    statusMessage = msg;
    statusMessageExpiry = chrono::steady_clock::now() + chrono::seconds(durationSeconds);
}

void App::setModuleEnabled(const string& modName, bool enabled) {
    if (modName == "cpu") modules.cpu = enabled;
    else if (modName == "mem") modules.mem = enabled;
    else if (modName == "disk") modules.disk = enabled;
    else if (modName == "net") modules.net = enabled;
    else if (modName == "proc") modules.proc = enabled;
}

void App::applyPreset(LayoutPreset preset) {
    currentPreset = preset;
    switch (preset) {
        case LayoutPreset::FULL:
            modules = {true, true, true, true, true};
            setStatus("Layout Preset: FULL (All Modules Active)", 2);
            break;
        case LayoutPreset::RESOURCES:
            modules = {true, true, true, true, false};
            setStatus("Layout Preset: RESOURCES (Graphs & Hardware Focus)", 2);
            break;
        case LayoutPreset::PROCESSES:
            modules = {false, false, false, false, true};
            setStatus("Layout Preset: PROCESSES (Fullscreen Table)", 2);
            break;
        case LayoutPreset::IO_FOCUS:
            modules = {false, false, true, true, true};
            setStatus("Layout Preset: I/O FOCUS (Disk, Network & Procs)", 2);
            break;
        case LayoutPreset::MINIMAL:
            modules = {true, true, false, false, false};
            setStatus("Layout Preset: MINIMAL (CPU & Memory Only)", 2);
            break;
        default:
            break;
    }
}

void App::cycleSelectedInterface(int direction) {
    lock_guard<mutex> lock(dataMutex);
    if (appData.netInfo.interfaces.empty()) return;
    int n = static_cast<int>(appData.netInfo.interfaces.size());
    selectedNetInterfaceIdx = (selectedNetInterfaceIdx + direction + n) % n;
    setStatus("Net Interface: " + appData.netInfo.interfaces[selectedNetInterfaceIdx].name, 2);
}

void App::collectorLoop() {
    SystemCPUInfo prevCpu = readSystemCPU();
    auto prevTime = chrono::steady_clock::now();
    this_thread::sleep_for(chrono::milliseconds(300));

    while (running.load()) {
        if (!paused.load()) {
            auto currTime = chrono::steady_clock::now();
            double deltaSec = chrono::duration<double>(currTime - prevTime).count();
            if (deltaSec <= 0.0) deltaSec = 1.0;
            prevTime = currTime;

            SystemCPUInfo currCpu = readSystemCPU();
            MemoryInfo mem = getMemoryInfo();
            SystemDiskInfo disk = readDiskStats(deltaSec);
            SystemNetInfo net = readNetStats(deltaSec);
            SensorInfo sensors = readSensors();

            long totalDelta = getTotalCPUTime(currCpu.total) - getTotalCPUTime(prevCpu.total);
            double totalCpuPercent = calculateCPUUsage(prevCpu.total, currCpu.total);

            vector<double> corePercents;
            size_t minCores = min(prevCpu.cores.size(), currCpu.cores.size());
            for (size_t i = 0; i < minCores; ++i) {
                corePercents.push_back(calculateCPUUsage(prevCpu.cores[i], currCpu.cores[i]));
            }

            int numCores = static_cast<int>(currCpu.cores.empty() ? 1 : currCpu.cores.size());
            ProcessSnapshot snap = getProcessesSnapshot(totalDelta, numCores, mem.totalBytes);

            {
                lock_guard<mutex> lock(dataMutex);
                appData.cpuInfo = currCpu;
                appData.totalCpuUsage = totalCpuPercent;
                appData.coreUsages = corePercents;
                appData.memInfo = mem;
                appData.snapshot = std::move(snap);
                appData.diskInfo = disk;
                appData.netInfo = net;
                appData.sensorInfo = sensors;

                cpuGraph.addSample(totalCpuPercent);
                memGraph.addSample(mem.memUsagePercent);
                diskReadGraph.addSample(static_cast<double>(disk.totalReadBytesSec));
                diskWriteGraph.addSample(static_cast<double>(disk.totalWriteBytesSec));
                netRxGraph.addSample(static_cast<double>(net.totalRxBytesSec));
                netTxGraph.addSample(static_cast<double>(net.totalTxBytesSec));
            }

            prevCpu = currCpu;
        }

        int interval = refreshIntervalMs.load();
        int step = 50;
        for (int elapsed = 0; elapsed < interval && running.load(); elapsed += step) {
            this_thread::sleep_for(chrono::milliseconds(step));
        }
    }
}

void App::run() {
    while (running.load()) {
        if (Terminal::wasResized()) {
            Terminal::clearResized();
        }

        KeyEvent evt = term.readKey(40);
        if (evt.code != KeyCode::NONE) {
            processInput(evt);
        }

        render();
    }
}

void App::sendSignalToSelected(int signalNum) {
    lock_guard<mutex> lock(dataMutex);
    vector<Process> procs;
    applySortAndFilter(appData.snapshot.processes, procs);

    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(procs.size())) {
        int targetPid = procs[selectedIndex].pid;
        if (sendProcessSignal(targetPid, signalNum) == 0) {
            setStatus("Signal " + to_string(signalNum) + " sent to PID " + to_string(targetPid), 3);
        } else {
            setStatus("Failed to send signal to PID " + to_string(targetPid), 3);
        }
    }
}

void App::processInput(const KeyEvent& evt) {
    if (searchMode) {
        if (evt.code == KeyCode::ESCAPE) {
            searchMode = false;
            searchQuery.clear();
            selectedIndex = 0;
            scrollOffset = 0;
        } else if (evt.code == KeyCode::ENTER) {
            searchMode = false;
        } else if (evt.code == KeyCode::BACKSPACE) {
            if (!searchQuery.empty()) {
                searchQuery.pop_back();
                selectedIndex = 0;
                scrollOffset = 0;
            }
        } else if (evt.code == KeyCode::CHAR) {
            searchQuery.push_back(evt.ch);
            selectedIndex = 0;
            scrollOffset = 0;
        }
        return;
    }

    if (activeModal != ModalType::NONE) {
        if (evt.code == KeyCode::ESCAPE || (evt.code == KeyCode::CHAR && evt.ch == 'q')) {
            activeModal = ModalType::NONE;
            modalSelectedIndex = 0;
            return;
        }

        if (activeModal == ModalType::MODULE_SELECT) {
            if (evt.code == KeyCode::UP || (evt.code == KeyCode::CHAR && evt.ch == 'k')) {
                modalSelectedIndex = max(0, modalSelectedIndex - 1);
            } else if (evt.code == KeyCode::DOWN || (evt.code == KeyCode::CHAR && evt.ch == 'j')) {
                modalSelectedIndex = min(5, modalSelectedIndex + 1);
            } else if (evt.code == KeyCode::ENTER || (evt.code == KeyCode::CHAR && evt.ch == ' ')) {
                switch (modalSelectedIndex) {
                    case 0: modules.cpu = !modules.cpu; break;
                    case 1: modules.mem = !modules.mem; break;
                    case 2: modules.disk = !modules.disk; break;
                    case 3: modules.net = !modules.net; break;
                    case 4: modules.proc = !modules.proc; break;
                    case 5:
                        int nextPreset = (static_cast<int>(currentPreset) + 1) % static_cast<int>(LayoutPreset::COUNT);
                        applyPreset(static_cast<LayoutPreset>(nextPreset));
                        break;
                }
            }
            return;
        } else if (activeModal == ModalType::SORT_SELECT) {
            if (evt.code == KeyCode::UP || (evt.code == KeyCode::CHAR && evt.ch == 'k')) {
                modalSelectedIndex = max(0, modalSelectedIndex - 1);
            } else if (evt.code == KeyCode::DOWN || (evt.code == KeyCode::CHAR && evt.ch == 'j')) {
                modalSelectedIndex = min(7, modalSelectedIndex + 1);
            } else if (evt.code == KeyCode::ENTER) {
                currentSort = static_cast<SortField>(modalSelectedIndex);
                activeModal = ModalType::NONE;
            }
            return;
        } else if (activeModal == ModalType::KILL_CONFIRM) {
            const int signals[] = {SIGTERM, SIGKILL, SIGHUP, SIGINT, SIGSTOP, SIGCONT};
            if (evt.code == KeyCode::UP || (evt.code == KeyCode::CHAR && evt.ch == 'k')) {
                modalSelectedIndex = max(0, modalSelectedIndex - 1);
            } else if (evt.code == KeyCode::DOWN || (evt.code == KeyCode::CHAR && evt.ch == 'j')) {
                modalSelectedIndex = min(5, modalSelectedIndex + 1);
            } else if (evt.code == KeyCode::ENTER) {
                sendSignalToSelected(signals[modalSelectedIndex]);
                activeModal = ModalType::NONE;
            }
            return;
        } else if (activeModal == ModalType::HELP) {
            if (evt.code == KeyCode::ENTER || evt.code == KeyCode::ESCAPE || evt.code == KeyCode::F1) {
                activeModal = ModalType::NONE;
            }
            return;
        }
    }

    switch (evt.code) {
        case KeyCode::CHAR:
            if (evt.ch == 'q' || evt.ch == 'Q') {
                running.store(false);
            } else if (evt.ch == '1') {
                modules.cpu = !modules.cpu;
                setStatus(string("CPU Module ") + (modules.cpu ? "Enabled" : "Disabled"), 2);
            } else if (evt.ch == '2') {
                modules.mem = !modules.mem;
                setStatus(string("Memory Module ") + (modules.mem ? "Enabled" : "Disabled"), 2);
            } else if (evt.ch == '3') {
                modules.disk = !modules.disk;
                setStatus(string("Disk Module ") + (modules.disk ? "Enabled" : "Disabled"), 2);
            } else if (evt.ch == '4') {
                modules.net = !modules.net;
                setStatus(string("Network Module ") + (modules.net ? "Enabled" : "Disabled"), 2);
            } else if (evt.ch == '5') {
                modules.proc = !modules.proc;
                setStatus(string("Process Module ") + (modules.proc ? "Enabled" : "Disabled"), 2);
            } else if (evt.ch == 'm' || evt.ch == 'M') {
                activeModal = (activeModal == ModalType::MODULE_SELECT) ? ModalType::NONE : ModalType::MODULE_SELECT;
                modalSelectedIndex = 0;
            } else if (evt.ch == '\t' || evt.ch == 'P') {
                int nextPreset = (static_cast<int>(currentPreset) + 1) % static_cast<int>(LayoutPreset::COUNT);
                applyPreset(static_cast<LayoutPreset>(nextPreset));
            } else if (evt.ch == 'i') {
                cycleSelectedInterface(1);
            } else if (evt.ch == 'I') {
                cycleSelectedInterface(-1);
            } else if (evt.ch == '+' || evt.ch == '=') {
                int cur = refreshIntervalMs.load();
                if (cur > 200) {
                    refreshIntervalMs.store(cur - 200);
                    setStatus("Refresh Rate: " + to_string(refreshIntervalMs.load()) + "ms", 2);
                }
            } else if (evt.ch == '-' || evt.ch == '_') {
                int cur = refreshIntervalMs.load();
                if (cur < 5000) {
                    refreshIntervalMs.store(cur + 200);
                    setStatus("Refresh Rate: " + to_string(refreshIntervalMs.load()) + "ms", 2);
                }
            } else if (evt.ch == ' ') {
                paused.store(!paused.load());
                setStatus(paused.load() ? "PAUSED (Space to resume)" : "RESUMED", 2);
            } else if (evt.ch == '/') {
                searchMode = true;
            } else if (evt.ch == 'c' || evt.ch == 'C') {
                currentSort = SortField::CPU;
                setStatus("Sorting by CPU%", 2);
            } else if (evt.ch == 'e' || evt.ch == 'E') {
                currentSort = SortField::MEM;
                setStatus("Sorting by MEM%", 2);
            } else if (evt.ch == 'p') {
                currentSort = SortField::PID;
                setStatus("Sorting by PID", 2);
            } else if (evt.ch == 't' || evt.ch == 'T') {
                currentSort = SortField::TIME;
                setStatus("Sorting by TIME+", 2);
            } else if (evt.ch == 'u' || evt.ch == 'U') {
                currentSort = SortField::USER;
                setStatus("Sorting by USER", 2);
            } else if (evt.ch == 'n' || evt.ch == 'N') {
                currentSort = SortField::NAME;
                setStatus("Sorting by Command Name", 2);
            } else if (evt.ch == 'r') {
                sortAscending = !sortAscending;
                setStatus(sortAscending ? "Sort Order: Ascending" : "Sort Order: Descending", 2);
            } else if (evt.ch == 'k' || evt.ch == 'K') {
                activeModal = ModalType::KILL_CONFIRM;
                modalSelectedIndex = 0;
            } else if (evt.ch == '?') {
                activeModal = ModalType::HELP;
            } else if (evt.ch == 'j') {
                selectedIndex++;
            } else if (evt.ch == 'k') {
                selectedIndex = max(0, selectedIndex - 1);
            } else if (evt.ch == 'g') {
                selectedIndex = 0;
                scrollOffset = 0;
            } else if (evt.ch == 'G') {
                selectedIndex = 999999;
            }
            break;

        case KeyCode::UP:
            selectedIndex = max(0, selectedIndex - 1);
            break;

        case KeyCode::DOWN:
            selectedIndex++;
            break;

        case KeyCode::PAGE_UP:
            selectedIndex = max(0, selectedIndex - 15);
            break;

        case KeyCode::PAGE_DOWN:
            selectedIndex += 15;
            break;

        case KeyCode::HOME:
            selectedIndex = 0;
            scrollOffset = 0;
            break;

        case KeyCode::END:
            selectedIndex = 999999;
            break;

        case KeyCode::F1:
            activeModal = (activeModal == ModalType::HELP) ? ModalType::NONE : ModalType::HELP;
            break;

        case KeyCode::F2:
            activeModal = (activeModal == ModalType::MODULE_SELECT) ? ModalType::NONE : ModalType::MODULE_SELECT;
            modalSelectedIndex = 0;
            break;

        case KeyCode::F3:
            searchMode = true;
            break;

        case KeyCode::F6:
            activeModal = (activeModal == ModalType::SORT_SELECT) ? ModalType::NONE : ModalType::SORT_SELECT;
            modalSelectedIndex = static_cast<int>(currentSort);
            break;

        case KeyCode::F9:
            activeModal = (activeModal == ModalType::KILL_CONFIRM) ? ModalType::NONE : ModalType::KILL_CONFIRM;
            modalSelectedIndex = 0;
            break;

        case KeyCode::F10:
            running.store(false);
            break;

        default:
            break;
    }
}

static string toLowerStr(const string& s) {
    string res = s;
    transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return tolower(c); });
    return res;
}

void App::applySortAndFilter(const vector<Process>& source, vector<Process>& dest) {
    dest.clear();
    string q = toLowerStr(searchQuery);

    for (const auto& p : source) {
        if (!q.empty()) {
            string pidStr = to_string(p.pid);
            if (pidStr.find(q) == string::npos &&
                toLowerStr(p.name).find(q) == string::npos &&
                toLowerStr(p.cmdline).find(q) == string::npos &&
                toLowerStr(p.user).find(q) == string::npos) {
                continue;
            }
        }
        dest.push_back(p);
    }

    stable_sort(dest.begin(), dest.end(), [this](const Process& a, const Process& b) {
        if (sortAscending) {
            switch (currentSort) {
                case SortField::CPU:
                    if (std::abs(a.cpu_usage - b.cpu_usage) > 0.05) return a.cpu_usage < b.cpu_usage;
                    break;
                case SortField::MEM:
                    if (std::abs(a.mem_usage - b.mem_usage) > 0.05) return a.mem_usage < b.mem_usage;
                    break;
                case SortField::PID:
                    if (a.pid != b.pid) return a.pid < b.pid;
                    break;
                case SortField::USER:
                    if (a.user != b.user) return a.user < b.user;
                    break;
                case SortField::TIME:
                    if (a.cpu_time_ticks != b.cpu_time_ticks) return a.cpu_time_ticks < b.cpu_time_ticks;
                    break;
                case SortField::NAME:
                    if (a.name != b.name) return a.name < b.name;
                    break;
                case SortField::VIRT:
                    if (a.virt_bytes != b.virt_bytes) return a.virt_bytes < b.virt_bytes;
                    break;
                case SortField::RES:
                    if (a.res_bytes != b.res_bytes) return a.res_bytes < b.res_bytes;
                    break;
            }
            if (a.res_bytes != b.res_bytes) return a.res_bytes < b.res_bytes;
            if (a.cpu_time_ticks != b.cpu_time_ticks) return a.cpu_time_ticks < b.cpu_time_ticks;
            return a.pid < b.pid;
        } else {
            switch (currentSort) {
                case SortField::CPU:
                    if (std::abs(a.cpu_usage - b.cpu_usage) > 0.05) return a.cpu_usage > b.cpu_usage;
                    break;
                case SortField::MEM:
                    if (std::abs(a.mem_usage - b.mem_usage) > 0.05) return a.mem_usage > b.mem_usage;
                    break;
                case SortField::PID:
                    if (a.pid != b.pid) return a.pid > b.pid;
                    break;
                case SortField::USER:
                    if (a.user != b.user) return a.user > b.user;
                    break;
                case SortField::TIME:
                    if (a.cpu_time_ticks != b.cpu_time_ticks) return a.cpu_time_ticks > b.cpu_time_ticks;
                    break;
                case SortField::NAME:
                    if (a.name != b.name) return a.name > b.name;
                    break;
                case SortField::VIRT:
                    if (a.virt_bytes != b.virt_bytes) return a.virt_bytes > b.virt_bytes;
                    break;
                case SortField::RES:
                    if (a.res_bytes != b.res_bytes) return a.res_bytes > b.res_bytes;
                    break;
            }
            if (a.res_bytes != b.res_bytes) return a.res_bytes > b.res_bytes;
            if (a.cpu_time_ticks != b.cpu_time_ticks) return a.cpu_time_ticks > b.cpu_time_ticks;
            return a.pid < b.pid;
        }
    });
}

LayoutBoxes App::computeLayout(int rows, int cols) {
    LayoutBoxes layout;

    layout.topBar = {1, 1, 1, cols};

    bool showSearch = searchMode || !searchQuery.empty() || (!statusMessage.empty() && chrono::steady_clock::now() < statusMessageExpiry);
    int footerH = 1;
    int searchH = showSearch ? 1 : 0;

    layout.footer = {rows, 1, footerH, cols};
    if (showSearch) {
        layout.searchBar = {rows - 1, 1, searchH, cols};
    }

    int innerY = 2;
    int innerH = rows - 1 - searchH - footerH;
    if (innerH < 3) innerH = 3;

    bool hasLeft = modules.cpu || modules.mem || modules.disk || modules.net;
    bool hasProc = modules.proc;

    if (!hasLeft && !hasProc) {
        layout.proc = {innerY, 1, innerH, cols};
        return layout;
    }

    size_t numCores = 0;
    {
        lock_guard<mutex> lock(dataMutex);
        numCores = appData.coreUsages.size();
    }
    int cpuWeight = (numCores >= 12) ? 6 : ((numCores >= 6) ? 5 : 4);

    if (!hasProc) {
        int curY = innerY;
        int remH = innerH;

        auto assignMod = [&](bool enabled, Rect& r, int weight, int totalWeights) {
            if (!enabled) return;
            int h = (remH * weight) / totalWeights;
            if (h < 4) h = min(4, remH);
            r = {curY, 1, h, cols};
            curY += h;
            remH -= h;
        };

        int weights = (modules.cpu ? cpuWeight : 0) + (modules.mem ? 3 : 0) + (modules.disk ? 3 : 0) + (modules.net ? 3 : 0);
        assignMod(modules.cpu, layout.cpu, cpuWeight, weights);
        assignMod(modules.mem, layout.mem, 3, weights);
        assignMod(modules.disk, layout.disk, 3, weights);
        if (modules.net) layout.net = {curY, 1, max(3, remH), cols};

        return layout;
    }

    if (!hasLeft) {
        layout.proc = {innerY, 1, innerH, cols};
        return layout;
    }

    if (cols >= 80) {
        int leftW = min(50, max(36, (cols * 38) / 100));
        int rightW = cols - leftW;
        int leftX = 1;
        int rightX = leftW + 1;

        layout.proc = {innerY, rightX, innerH, rightW};

        vector<pair<string, int>> activeLeft;
        if (modules.cpu) activeLeft.push_back({"cpu", cpuWeight});
        if (modules.mem) activeLeft.push_back({"mem", 3});
        if (modules.disk) activeLeft.push_back({"disk", 3});
        if (modules.net) activeLeft.push_back({"net", 3});

        int totalWeight = 0;
        for (const auto& m : activeLeft) totalWeight += m.second;

        int curY = innerY;
        int remH = innerH;

        for (size_t i = 0; i < activeLeft.size(); ++i) {
            int h = (i == activeLeft.size() - 1) ? remH : ((innerH * activeLeft[i].second) / totalWeight);
            if (h < 3) h = min(3, remH);

            if (activeLeft[i].first == "cpu") layout.cpu = {curY, leftX, h, leftW};
            else if (activeLeft[i].first == "mem") layout.mem = {curY, leftX, h, leftW};
            else if (activeLeft[i].first == "disk") layout.disk = {curY, leftX, h, leftW};
            else if (activeLeft[i].first == "net") layout.net = {curY, leftX, h, leftW};

            curY += h;
            remH -= h;
        }
    } else {
        int topH = innerH / 2;
        int botH = innerH - topH;

        layout.proc = {innerY + topH, 1, botH, cols};

        int curY = innerY;
        int remH = topH;
        int count = (modules.cpu ? 1 : 0) + (modules.mem ? 1 : 0) + (modules.disk ? 1 : 0) + (modules.net ? 1 : 0);
        int perH = count > 0 ? (topH / count) : topH;

        if (modules.cpu) { layout.cpu = {curY, 1, min(perH, remH), cols}; curY += layout.cpu.h; remH -= layout.cpu.h; }
        if (modules.mem) { layout.mem = {curY, 1, min(perH, remH), cols}; curY += layout.mem.h; remH -= layout.mem.h; }
        if (modules.disk) { layout.disk = {curY, 1, min(perH, remH), cols}; curY += layout.disk.h; remH -= layout.disk.h; }
        if (modules.net) { layout.net = {curY, 1, max(2, remH), cols}; }
    }

    return layout;
}

void App::render() {
    TermSize size = term.getSize();
    RenderBuffer buf(size.rows, size.cols);
    buf.clear();

    AppData dataCopy;
    {
        lock_guard<mutex> lock(dataMutex);
        dataCopy = appData;
    }

    vector<Process> visibleProcs;
    applySortAndFilter(dataCopy.snapshot.processes, visibleProcs);

    if (visibleProcs.empty()) {
        selectedIndex = 0;
        scrollOffset = 0;
    } else {
        if (selectedIndex >= static_cast<int>(visibleProcs.size())) {
            selectedIndex = static_cast<int>(visibleProcs.size()) - 1;
        }
        if (selectedIndex < 0) selectedIndex = 0;
    }

    LayoutBoxes layout = computeLayout(size.rows, size.cols);

    renderTopBar(buf, layout.topBar, dataCopy);

    if (layout.cpu.isValid()) renderCpuPanel(buf, layout.cpu, dataCopy);
    if (layout.mem.isValid()) renderMemPanel(buf, layout.mem, dataCopy);
    if (layout.disk.isValid()) renderDiskPanel(buf, layout.disk, dataCopy);
    if (layout.net.isValid()) renderNetPanel(buf, layout.net, dataCopy);
    if (layout.proc.isValid()) renderProcPanel(buf, layout.proc, visibleProcs);

    if (layout.searchBar.isValid()) {
        if (searchMode || !searchQuery.empty()) {
            string searchStyle = Color::BG_BRIGHT_BLACK + Color::FG_BRIGHT_WHITE;
            buf.fillRow(layout.searchBar.y, ' ', searchStyle);
            string prompt = searchMode ? " 🔍 SEARCH (Enter to confirm, Esc to clear): " : " 🔍 FILTER ACTIVE: ";
            buf.writeText(layout.searchBar.y, 1, prompt + searchQuery, searchStyle + Color::BOLD);
        } else if (!statusMessage.empty() && chrono::steady_clock::now() < statusMessageExpiry) {
            string msgStyle = Color::BG_BLUE + Color::FG_BRIGHT_WHITE + Color::BOLD;
            buf.fillRow(layout.searchBar.y, ' ', msgStyle);
            buf.writeText(layout.searchBar.y, 2, " ℹ " + statusMessage + " ", msgStyle);
        }
    }

    renderFooter(buf, layout.footer);
    renderModals(buf);

    buf.flush();
}

void App::renderTopBar(RenderBuffer& buf, const Rect& rect, const AppData& data) {
    string barStyle = Color::BG_BLACK + Color::FG_WHITE;
    buf.fillRow(rect.y, ' ', barStyle);

    ostringstream ss;

    auto addModBadge = [&](const string& key, const string& name, bool active) {
        if (active) {
            ss << Color::BG_CYAN << Color::FG_BLACK << Color::BOLD << " " << key << ":" << name << "✓ " << Color::RESET << barStyle << " ";
        } else {
            ss << Color::FG_BRIGHT_BLACK << "[" << key << ":" << name << "] " << Color::RESET << barStyle;
        }
    };

    ss << Color::BOLD << Color::FG_BRIGHT_CYAN << "btop++ " << Color::RESET << barStyle;
    addModBadge("1", "cpu", modules.cpu);
    addModBadge("2", "mem", modules.mem);
    addModBadge("3", "disk", modules.disk);
    addModBadge("4", "net", modules.net);
    addModBadge("5", "proc", modules.proc);

    ss << " │ " << Color::FG_YELLOW << Color::BOLD << "Load: " << Color::RESET << barStyle
       << fixed << setprecision(2) << data.cpuInfo.load1 << " " << data.cpuInfo.load5 << " " << data.cpuInfo.load15;

    if (data.sensorInfo.isAvailable && data.sensorInfo.cpuTempC > 0) {
        string tempCol = data.sensorInfo.cpuTempC > 80.0 ? Color::FG_BRIGHT_RED :
                        (data.sensorInfo.cpuTempC > 65.0 ? Color::FG_BRIGHT_YELLOW : Color::FG_BRIGHT_GREEN);
        ss << " │ " << Color::BOLD << "Temp: " << tempCol << fixed << setprecision(1) << data.sensorInfo.cpuTempC << "°C" << Color::RESET << barStyle;
    }

    ss << " │ " << Color::FG_MAGENTA << Color::BOLD << "Up: " << Color::RESET << barStyle
       << RenderBuffer::formatTime(data.cpuInfo.uptimeSeconds);

    buf.writeText(rect.y, 1, ss.str(), barStyle);

    auto t = time(nullptr);
    auto tm = *localtime(&t);
    ostringstream timeSS;
    timeSS << setfill('0') << setw(2) << tm.tm_hour << ":"
           << setfill('0') << setw(2) << tm.tm_min << ":"
           << setfill('0') << setw(2) << tm.tm_sec;

    string rightInfo = "⏱ " + to_string(refreshIntervalMs.load()) + "ms  " + timeSS.str() + " ";
    int rightCol = rect.w - static_cast<int>(rightInfo.length()) + 1;
    if (rightCol > 40) {
        buf.writeText(rect.y, rightCol, rightInfo, Color::FG_BRIGHT_BLACK + Color::BOLD);
    }
}

void App::renderCpuPanel(RenderBuffer& buf, const Rect& rect, const AppData& data) {
    ostringstream badgeSS;
    badgeSS << fixed << setprecision(1) << data.totalCpuUsage << "%";
    buf.drawRoundedBox(rect, "CPU", badgeSS.str(), Color::FG_CYAN, Color::BOLD + Color::FG_BRIGHT_CYAN);

    int innerY = rect.innerY();
    int innerX = rect.innerX();
    int innerH = rect.innerH();
    int innerW = rect.innerW();
    if (innerH <= 0 || innerW <= 4) return;

    size_t numCores = data.coreUsages.size();
    int curY = innerY;

    if (numCores == 0) {
        buf.drawGradientBar(curY++, innerX, innerW, data.totalCpuUsage, "Avg CPU", "%", 0, 180, 255, 255, 60, 100);
    } else {
        int colsCount = 2;
        if (innerW >= 66 && numCores >= 12) colsCount = 3;
        else if (innerW < 32) colsCount = 1;

        int colW = (innerW - (colsCount - 1) * 2) / colsCount;
        int rowsNeeded = static_cast<int>((numCores + colsCount - 1) / colsCount);
        int rowsToDraw = min(rowsNeeded, max(1, innerH - 1));

        for (int r = 0; r < rowsToDraw && curY < innerY + innerH; ++r) {
            for (int c = 0; c < colsCount; ++c) {
                size_t coreIdx = c * rowsNeeded + r;
                if (coreIdx < numCores) {
                    int x = innerX + c * (colW + 2);
                    ostringstream lss;
                    lss << "C" << setw(numCores >= 10 ? 2 : 1) << (coreIdx + 1);
                    buf.drawGradientBar(curY, x, colW, data.coreUsages[coreIdx], lss.str(), "%", 0, 200, 255, 255, 80, 80);
                }
            }
            curY++;
        }
    }

    int graphH = (innerY + innerH) - curY;
    if (graphH >= 1) {
        vector<string> matrix = cpuGraph.renderBrailleMatrix(graphH, innerW, 0.0, 100.0, Color::FG_BRIGHT_RED, Color::FG_BRIGHT_CYAN);
        for (int r = 0; r < graphH; ++r) {
            buf.writeText(curY + r, innerX, matrix[r]);
        }
    }
}

void App::renderMemPanel(RenderBuffer& buf, const Rect& rect, const AppData& data) {
    ostringstream badgeSS;
    badgeSS << RenderBuffer::formatBytes(data.memInfo.usedBytes) << " / " << RenderBuffer::formatBytes(data.memInfo.totalBytes);
    buf.drawRoundedBox(rect, "MEM", badgeSS.str(), Color::FG_MAGENTA, Color::BOLD + Color::FG_BRIGHT_MAGENTA);

    int innerY = rect.innerY();
    int innerX = rect.innerX();
    int innerH = rect.innerH();
    int innerW = rect.innerW();
    if (innerH <= 0 || innerW <= 4) return;

    int curY = innerY;

    buf.drawGradientBar(curY++, innerX, innerW, data.memInfo.memUsagePercent, "RAM", "%", 140, 60, 255, 255, 60, 180);

    if (curY < innerY + innerH) {
        ostringstream memDetail;
        memDetail << Color::FG_BRIGHT_BLACK << "Used:" << Color::RESET << RenderBuffer::formatBytes(data.memInfo.usedBytes)
                  << " " << Color::FG_BRIGHT_BLACK << "Free:" << Color::RESET << RenderBuffer::formatBytes(data.memInfo.freeBytes)
                  << " " << Color::FG_BRIGHT_BLACK << "Avail:" << Color::RESET << RenderBuffer::formatBytes(data.memInfo.availableBytes);
        buf.writeTextClipped(curY++, innerX, innerW, memDetail.str());
    }

    if (curY < innerY + innerH && data.memInfo.swapTotalBytes > 0) {
        buf.drawGradientBar(curY++, innerX, innerW, data.memInfo.swapUsagePercent, "SWP", "%", 60, 200, 120, 255, 120, 0);
    }

    int graphH = (innerY + innerH) - curY;
    if (graphH >= 1) {
        vector<string> matrix = memGraph.renderBrailleMatrix(graphH, innerW, 0.0, 100.0, Color::FG_BRIGHT_MAGENTA, Color::FG_BRIGHT_BLUE);
        for (int r = 0; r < graphH; ++r) {
            buf.writeText(curY + r, innerX, matrix[r]);
        }
    }
}

void App::renderDiskPanel(RenderBuffer& buf, const Rect& rect, const AppData& data) {
    ostringstream badgeSS;
    badgeSS << "▲ " << RenderBuffer::formatRate(data.diskInfo.totalReadBytesSec)
            << " ▼ " << RenderBuffer::formatRate(data.diskInfo.totalWriteBytesSec);
    buf.drawRoundedBox(rect, "DISK", badgeSS.str(), Color::FG_YELLOW, Color::BOLD + Color::FG_BRIGHT_YELLOW);

    int innerY = rect.innerY();
    int innerX = rect.innerX();
    int innerH = rect.innerH();
    int innerW = rect.innerW();
    if (innerH <= 0 || innerW <= 4) return;

    int curY = innerY;

    size_t mountCount = data.diskInfo.mounts.size();
    int maxMountsToShow = min(static_cast<int>(mountCount), max(1, innerH - 2));

    for (int i = 0; i < maxMountsToShow && curY < innerY + innerH; ++i) {
        const auto& m = data.diskInfo.mounts[i];
        ostringstream mLabel;
        mLabel << m.mountPoint;
        string valSuffix = "% (" + RenderBuffer::formatBytes(m.usedBytes) + "/" + RenderBuffer::formatBytes(m.totalBytes) + ")";
        buf.drawGradientBar(curY++, innerX, innerW, m.usedPercent, mLabel.str(), valSuffix, 255, 180, 0, 255, 60, 60);
    }

    int graphH = (innerY + innerH) - curY;
    if (graphH >= 1) {
        double maxRate = max(1024.0 * 1024.0, max(diskReadGraph.getMax(), diskWriteGraph.getMax()));
        string readSpark = diskReadGraph.renderBrailleLine(innerW / 2, 0.0, maxRate, Color::FG_BRIGHT_GREEN);
        string writeSpark = diskWriteGraph.renderBrailleLine(innerW - (innerW / 2) - 1, 0.0, maxRate, Color::FG_BRIGHT_YELLOW);

        buf.writeText(curY, innerX, "R:" + readSpark + " W:" + writeSpark);
    }
}

void App::renderNetPanel(RenderBuffer& buf, const Rect& rect, const AppData& data) {
    string activeIface = "all";
    uint64_t rxSec = data.netInfo.totalRxBytesSec;
    uint64_t txSec = data.netInfo.totalTxBytesSec;
    uint64_t totalRx = 0, totalTx = 0;

    if (!data.netInfo.interfaces.empty()) {
        int idx = clamp(selectedNetInterfaceIdx, 0, static_cast<int>(data.netInfo.interfaces.size() - 1));
        activeIface = data.netInfo.interfaces[idx].name;
        rxSec = data.netInfo.interfaces[idx].rxBytesSec;
        txSec = data.netInfo.interfaces[idx].txBytesSec;
        totalRx = data.netInfo.interfaces[idx].totalRxBytes;
        totalTx = data.netInfo.interfaces[idx].totalTxBytes;
    }

    ostringstream badgeSS;
    badgeSS << "[" << activeIface << "] ▼ " << RenderBuffer::formatRate(rxSec)
            << " ▲ " << RenderBuffer::formatRate(txSec);
    buf.drawRoundedBox(rect, "NET", badgeSS.str(), Color::FG_GREEN, Color::BOLD + Color::FG_BRIGHT_GREEN);

    int innerY = rect.innerY();
    int innerX = rect.innerX();
    int innerH = rect.innerH();
    int innerW = rect.innerW();
    if (innerH <= 0 || innerW <= 4) return;

    int curY = innerY;

    ostringstream rateSS;
    rateSS << Color::FG_BRIGHT_CYAN << "▼ RX: " << Color::BOLD << RenderBuffer::formatRate(rxSec) << Color::RESET
           << "  " << Color::FG_BRIGHT_MAGENTA << "▲ TX: " << Color::BOLD << RenderBuffer::formatRate(txSec) << Color::RESET
           << "  " << Color::FG_BRIGHT_BLACK << "Tot: " << RenderBuffer::formatBytes(totalRx) << "/" << RenderBuffer::formatBytes(totalTx);
    buf.writeTextClipped(curY++, innerX, innerW, rateSS.str());

    int graphH = (innerY + innerH) - curY;
    if (graphH >= 1) {
        double maxNetRate = max(100.0 * 1024.0, max(netRxGraph.getMax(), netTxGraph.getMax()));
        vector<string> rxMatrix = netRxGraph.renderBrailleMatrix(graphH, innerW, 0.0, maxNetRate, Color::FG_BRIGHT_CYAN, Color::FG_BLUE);
        for (int r = 0; r < graphH; ++r) {
            buf.writeText(curY + r, innerX, rxMatrix[r]);
        }
    }
}

void App::renderProcPanel(RenderBuffer& buf, const Rect& rect, const vector<Process>& procs) {
    ostringstream badgeSS;
    badgeSS << procs.size() << " procs";
    if (selectedIndex < static_cast<int>(procs.size())) {
        badgeSS << " (PID " << procs[selectedIndex].pid << ")";
    }
    buf.drawRoundedBox(rect, "PROC", badgeSS.str(), Color::FG_BLUE, Color::BOLD + Color::FG_BRIGHT_BLUE);

    int innerY = rect.innerY();
    int innerX = rect.innerX();
    int innerH = rect.innerH();
    int innerW = rect.innerW();
    if (innerH <= 1 || innerW <= 10) return;

    string sortArrow = sortAscending ? "▲" : "▼";

    struct Col {
        string name;
        SortField field;
        int width;
        bool alignRight;
    };

    vector<Col> cols;
    if (innerW >= 80) {
        cols = {
            {"PID", SortField::PID, 7, false},
            {"USER", SortField::USER, 9, false},
            {"PRI", SortField::PID, 4, true},
            {"NI", SortField::PID, 4, true},
            {"VIRT", SortField::VIRT, 8, true},
            {"RES", SortField::RES, 8, true},
            {"S", SortField::PID, 2, false},
            {"CPU%", SortField::CPU, 7, true},
            {"MEM%", SortField::MEM, 7, true},
            {"TIME+", SortField::TIME, 9, true}
        };
    } else if (innerW >= 56) {
        cols = {
            {"PID", SortField::PID, 7, false},
            {"USER", SortField::USER, 8, false},
            {"RES", SortField::RES, 8, true},
            {"S", SortField::PID, 2, false},
            {"CPU%", SortField::CPU, 7, true},
            {"MEM%", SortField::MEM, 7, true},
            {"TIME+", SortField::TIME, 8, true}
        };
    } else {
        cols = {
            {"PID", SortField::PID, 6, false},
            {"USER", SortField::USER, 7, false},
            {"RES", SortField::RES, 7, true},
            {"CPU%", SortField::CPU, 6, true},
            {"MEM%", SortField::MEM, 6, true}
        };
    }

    ostringstream hdr;
    for (const auto& c : cols) {
        string title = c.name;
        if (currentSort == c.field) title += sortArrow;
        string cell = RenderBuffer::truncateOrPad(title, c.width, !c.alignRight);
        if (currentSort == c.field) {
            hdr << Color::BG_CYAN << Color::FG_BLACK << Color::BOLD << cell << Color::BG_BLUE << Color::FG_WHITE << " ";
        } else {
            hdr << cell << " ";
        }
    }
    hdr << "Command";

    buf.writeTextClipped(innerY, innerX, innerW, hdr.str(), Color::BG_BLUE + Color::FG_WHITE + Color::BOLD);

    int maxRows = innerH - 1;
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + maxRows) {
        scrollOffset = selectedIndex - maxRows + 1;
    }

    int printRow = innerY + 1;
    for (int i = 0; i < maxRows; ++i) {
        int procIdx = scrollOffset + i;
        if (procIdx >= static_cast<int>(procs.size())) {
            buf.writeText(printRow++, innerX, string(innerW, ' '));
            continue;
        }

        const auto& p = procs[procIdx];
        bool isSelected = (procIdx == selectedIndex);

        string rowStyle;
        if (isSelected) {
            rowStyle = Color::BG_BRIGHT_BLUE + Color::FG_BRIGHT_WHITE + Color::BOLD;
        } else if (i % 2 == 1) {
            rowStyle = Color::rgb(20, 24, 32, true);
        }

        ostringstream rowSS;
        for (const auto& c : cols) {
            string val;
            if (c.name == "PID") val = to_string(p.pid);
            else if (c.name == "USER") val = p.user;
            else if (c.name == "PRI") val = to_string(p.priority);
            else if (c.name == "NI") val = to_string(p.nice);
            else if (c.name == "VIRT") val = RenderBuffer::formatBytes(p.virt_bytes);
            else if (c.name == "RES") val = RenderBuffer::formatBytes(p.res_bytes);
            else if (c.name == "S") val = string(1, p.state);
            else if (c.name == "CPU%") {
                ostringstream cs;
                cs << fixed << setprecision(1) << p.cpu_usage;
                val = cs.str();
            } else if (c.name == "MEM%") {
                ostringstream ms;
                ms << fixed << setprecision(1) << p.mem_usage;
                val = ms.str();
            } else if (c.name == "TIME+") {
                val = RenderBuffer::formatTime(p.cpu_time_seconds);
            }

            string cell = RenderBuffer::truncateOrPad(val, c.width, !c.alignRight);

            if (!isSelected && c.name == "CPU%" && p.cpu_usage > 40.0) {
                rowSS << Color::FG_BRIGHT_RED << Color::BOLD << cell << Color::RESET;
                if (!rowStyle.empty()) rowSS << rowStyle;
            } else if (!isSelected && c.name == "CPU%" && p.cpu_usage > 15.0) {
                rowSS << Color::FG_BRIGHT_YELLOW << cell << Color::RESET;
                if (!rowStyle.empty()) rowSS << rowStyle;
            } else if (!isSelected && c.name == "MEM%" && p.mem_usage > 30.0) {
                rowSS << Color::FG_BRIGHT_MAGENTA << cell << Color::RESET;
                if (!rowStyle.empty()) rowSS << rowStyle;
            } else {
                rowSS << cell;
            }
            rowSS << " ";
        }

        rowSS << p.cmdline;
        buf.writeTextClipped(printRow++, innerX, innerW, rowSS.str(), rowStyle);
    }
}

void App::renderFooter(RenderBuffer& buf, const Rect& rect) {
    string fStyle = Color::BG_BLACK + Color::FG_WHITE;
    buf.fillRow(rect.y, ' ', fStyle);

    ostringstream ss;
    auto addKey = [&](const string& k, const string& desc) {
        ss << Color::BG_CYAN << Color::FG_BLACK << Color::BOLD << " " << k << " " << Color::RESET
           << Color::BG_BLACK << Color::FG_WHITE << desc << " ";
    };

    addKey("1-5", "Modules");
    addKey("Tab", "Preset");
    addKey("i", "Iface");
    addKey("/", "Search");
    addKey("c/e/p/t", "Sort");
    addKey("k", "Kill");
    addKey("m", "Menu");
    addKey("F1", "Help");
    addKey("q", "Quit");

    buf.writeText(rect.y, 1, ss.str(), fStyle);
}

void App::renderModals(RenderBuffer& buf) {
    if (activeModal == ModalType::NONE) return;

    int cols = buf.getCols();
    int rows = buf.getRows();

    if (activeModal == ModalType::MODULE_SELECT) {
        int width = 46;
        int height = 12;
        int startX = (cols - width) / 2;
        int startY = (rows - height) / 2;

        buf.drawRoundedBox(startY, startX, height, width, " Module Configuration ", "", Color::FG_BRIGHT_CYAN, Color::BG_BLACK + Color::FG_CYAN + Color::BOLD);
        for (int r = startY + 1; r < startY + height - 1; ++r) {
            buf.writeText(r, startX + 1, string(width - 2, ' '), Color::BG_BLACK);
        }

        const char* modLabels[] = {
            modules.cpu ? "[x] CPU Monitor (Usage, Cores, Graph)" : "[ ] CPU Monitor (Usage, Cores, Graph)",
            modules.mem ? "[x] Memory Monitor (RAM, Swap, Graph)" : "[ ] Memory Monitor (RAM, Swap, Graph)",
            modules.disk ? "[x] Disk Monitor (Mounts, IO, Rates)" : "[ ] Disk Monitor (Mounts, IO, Rates)",
            modules.net ? "[x] Network Monitor (RX/TX, Graphs)" : "[ ] Network Monitor (RX/TX, Graphs)",
            modules.proc ? "[x] Process Monitor (Table, Signals)" : "[ ] Process Monitor (Table, Signals)",
            "Cycle Layout Preset (Tab / P)"
        };

        for (int i = 0; i < 6; ++i) {
            int r = startY + 2 + i;
            bool isSel = (i == modalSelectedIndex);
            string style = isSel ? (Color::BG_CYAN + Color::FG_BLACK + Color::BOLD) : (Color::BG_BLACK + Color::FG_WHITE);
            string line = string(isSel ? " ▶ " : "   ") + modLabels[i];
            buf.writeText(r, startX + 2, RenderBuffer::truncateOrPad(line, width - 4, true), style);
        }
    } else if (activeModal == ModalType::HELP) {
        int width = min(72, cols - 4);
        int height = min(22, rows - 4);
        int startX = (cols - width) / 2;
        int startY = (rows - height) / 2;

        buf.drawRoundedBox(startY, startX, height, width, " Help & Keyboard Shortcuts ", "", Color::FG_CYAN, Color::BG_BLACK + Color::FG_CYAN + Color::BOLD);
        for (int r = startY + 1; r < startY + height - 1; ++r) {
            buf.writeText(r, startX + 1, string(width - 2, ' '), Color::BG_BLACK);
        }

        int r = startY + 1;
        auto addHelp = [&](const string& key, const string& desc) {
            if (r < startY + height - 1) {
                buf.writeText(r, startX + 3, key, Color::BG_BLACK + Color::FG_YELLOW + Color::BOLD);
                buf.writeText(r++, startX + 22, desc, Color::BG_BLACK + Color::FG_WHITE);
            }
        };

        addHelp("1, 2, 3, 4, 5", "Toggle CPU, MEM, DISK, NET, PROC panels");
        addHelp("Tab / P", "Cycle layout presets (Full, Resources, Proc, Minimal)");
        addHelp("m / F2", "Open Module configuration modal");
        addHelp("i / I", "Cycle network interface (eth0, wlan0, etc.)");
        addHelp("+ / -", "Increase / decrease update frequency");
        addHelp("Up / Down, j/k", "Navigate process list");
        addHelp("PgUp / PgDn", "Scroll 15 processes");
        addHelp("Home / End, g/G", "Jump to top / bottom");
        addHelp("/ or F3", "Live substring search / filter");
        addHelp("c / e / p / t", "Sort by CPU%, MEM%, PID, TIME+");
        addHelp("u / n", "Sort by USER, Command Name");
        addHelp("r", "Invert sort direction (asc/desc)");
        addHelp("F6", "Open interactive sort menu");
        addHelp("F9 or k", "Send signal (SIGTERM/KILL) to process");
        addHelp("Space", "Pause / Resume live monitoring");
        addHelp("q / F10", "Quit process monitor");
    } else if (activeModal == ModalType::SORT_SELECT) {
        int width = 36;
        int height = 12;
        int startX = (cols - width) / 2;
        int startY = (rows - height) / 2;

        buf.drawRoundedBox(startY, startX, height, width, " Select Sort Field ", "", Color::FG_CYAN, Color::BG_BLACK + Color::FG_CYAN + Color::BOLD);
        for (int r = startY + 1; r < startY + height - 1; ++r) {
            buf.writeText(r, startX + 1, string(width - 2, ' '), Color::BG_BLACK);
        }

        const char* sortNames[] = {
            "CPU% Utilization", "Memory% Usage", "PID (Process ID)",
            "Username", "Total CPU Time", "Command Name",
            "Virtual Memory (VIRT)", "Resident Memory (RES)"
        };

        for (int i = 0; i < 8; ++i) {
            int r = startY + 2 + i;
            bool isSel = (i == modalSelectedIndex);
            string style = isSel ? (Color::BG_CYAN + Color::FG_BLACK + Color::BOLD) : (Color::BG_BLACK + Color::FG_WHITE);
            string line = string(isSel ? " ▶ " : "   ") + sortNames[i];
            buf.writeText(r, startX + 2, RenderBuffer::truncateOrPad(line, width - 4, true), style);
        }
    } else if (activeModal == ModalType::KILL_CONFIRM) {
        int width = 38;
        int height = 10;
        int startX = (cols - width) / 2;
        int startY = (rows - height) / 2;

        buf.drawRoundedBox(startY, startX, height, width, " Send Signal to Process ", "", Color::FG_RED, Color::BG_BLACK + Color::FG_RED + Color::BOLD);
        for (int r = startY + 1; r < startY + height - 1; ++r) {
            buf.writeText(r, startX + 1, string(width - 2, ' '), Color::BG_BLACK);
        }

        const char* signalNames[] = {
            "15) SIGTERM (Graceful Terminate)",
            " 9) SIGKILL (Force Kill)",
            " 1) SIGHUP  (Reload/Hangup)",
            " 2) SIGINT  (Interrupt)",
            "19) SIGSTOP (Pause Process)",
            "18) SIGCONT (Resume Process)"
        };

        for (int i = 0; i < 6; ++i) {
            int r = startY + 2 + i;
            bool isSel = (i == modalSelectedIndex);
            string style = isSel ? (Color::BG_RED + Color::FG_WHITE + Color::BOLD) : (Color::BG_BLACK + Color::FG_WHITE);
            string line = string(isSel ? " ▶ " : "   ") + signalNames[i];
            buf.writeText(r, startX + 2, RenderBuffer::truncateOrPad(line, width - 4, true), style);
        }
    }
}
