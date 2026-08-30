#include "app.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <sys/types.h>

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
            }

            prevCpu = currCpu;
        }

        for (int i = 0; i < 10 && running.load(); ++i) {
            this_thread::sleep_for(chrono::milliseconds(100));
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
        if (kill(targetPid, signalNum) == 0) {
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

        if (activeModal == ModalType::SORT_SELECT) {
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
            } else if (evt.ch == ' ') {
                paused.store(!paused.load());
                setStatus(paused.load() ? "PAUSED (Space to resume)" : "RESUMED", 2);
            } else if (evt.ch == '/') {
                searchMode = true;
            } else if (evt.ch == 'c' || evt.ch == 'C') {
                currentSort = SortField::CPU;
                setStatus("Sorting by CPU%", 2);
            } else if (evt.ch == 'm' || evt.ch == 'M') {
                currentSort = SortField::MEM;
                setStatus("Sorting by MEM%", 2);
            } else if (evt.ch == 'p' || evt.ch == 'P') {
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
            } else if (evt.ch == 'r' || evt.ch == 'I') {
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
                    if (std::abs(a.cpu_usage - b.cpu_usage) > 0.01) return a.cpu_usage < b.cpu_usage;
                    break;
                case SortField::MEM:
                    if (std::abs(a.mem_usage - b.mem_usage) > 0.01) return a.mem_usage < b.mem_usage;
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
            if (a.cpu_time_ticks != b.cpu_time_ticks) return a.cpu_time_ticks < b.cpu_time_ticks;
            return a.pid < b.pid;
        } else {
            switch (currentSort) {
                case SortField::CPU:
                    if (std::abs(a.cpu_usage - b.cpu_usage) > 0.01) return a.cpu_usage > b.cpu_usage;
                    break;
                case SortField::MEM:
                    if (std::abs(a.mem_usage - b.mem_usage) > 0.01) return a.mem_usage > b.mem_usage;
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
            if (a.cpu_time_ticks != b.cpu_time_ticks) return a.cpu_time_ticks > b.cpu_time_ticks;
            return a.pid < b.pid;
        }
    });
}

void App::render() {
    TermSize size = term.getSize();
    RenderBuffer buf(size.rows, size.cols);
    buf.clear();

    AppData dataCopy;
    string cpuSparkline;
    string memSparkline;

    {
        lock_guard<mutex> lock(dataMutex);
        dataCopy = appData;
        size_t graphWidth = static_cast<size_t>(max(10, size.cols / 4 - 4));
        cpuSparkline = cpuGraph.renderBrailleLine(graphWidth, 0.0, 100.0, Color::FG_BRIGHT_CYAN);
        memSparkline = memGraph.renderBrailleLine(graphWidth, 0.0, 100.0, Color::FG_BRIGHT_MAGENTA);
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

    int curRow = 1;
    drawHeader(buf, dataCopy, curRow);

    int tableStartRow = curRow;
    int footerHeight = (searchMode || !searchQuery.empty() || !statusMessage.empty()) ? 2 : 1;
    int availableRows = size.rows - tableStartRow - footerHeight;

    if (availableRows > 0) {
        if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        } else if (selectedIndex >= scrollOffset + availableRows) {
            scrollOffset = selectedIndex - availableRows + 1;
        }
        drawProcessTable(buf, visibleProcs, tableStartRow, availableRows);
    }

    int footerRow = size.rows;
    if (searchMode || !searchQuery.empty()) {
        string searchStyle = Color::BG_BRIGHT_BLACK + Color::FG_BRIGHT_WHITE;
        buf.fillRow(size.rows - 1, ' ', searchStyle);
        string searchPrompt = searchMode ? " SEARCH (Type query, Enter/Esc): " : " FILTER: ";
        buf.writeText(size.rows - 1, 1, searchPrompt + searchQuery, searchStyle + Color::BOLD);
    } else if (!statusMessage.empty() && chrono::steady_clock::now() < statusMessageExpiry) {
        string msgStyle = Color::BG_BLUE + Color::FG_BRIGHT_WHITE + Color::BOLD;
        buf.fillRow(size.rows - 1, ' ', msgStyle);
        buf.writeText(size.rows - 1, 2, " " + statusMessage + " ", msgStyle);
    }

    drawFooter(buf, footerRow);
    drawModals(buf);

    buf.flush();
}

void App::drawHeader(RenderBuffer& buf, const AppData& data, int& curRow) {
    int cols = buf.getCols();
    int rows = buf.getRows();

    size_t numCores = data.coreUsages.size();

    if (cols >= 70) {
        int colWidth = (cols / 2) - 2;
        int leftCol = 1;
        int rightCol = colWidth + 4;

        int leftRow = curRow;
        int rightRow = curRow;

        if (numCores == 0) {
            buf.drawProgressBar(leftRow++, leftCol, colWidth, data.totalCpuUsage, "CPU Avg");
        } else {
            size_t leftCount = (numCores + 1) / 2;
            size_t maxCpuRows = (rows > 28) ? leftCount : min(leftCount, static_cast<size_t>(6));

            for (size_t i = 0; i < maxCpuRows; ++i) {
                ostringstream lss;
                lss << "Core " << setw(2) << (i + 1);
                buf.drawProgressBar(leftRow++, leftCol, colWidth, data.coreUsages[i], lss.str());
            }

            for (size_t i = leftCount; i < leftCount + maxCpuRows && i < numCores; ++i) {
                ostringstream lss;
                lss << "Core " << setw(2) << (i + 1);
                buf.drawProgressBar(rightRow++, rightCol, colWidth, data.coreUsages[i], lss.str());
            }
        }

        curRow = max(leftRow, rightRow);

        // Memory & Swap Gauges
        string memLabel = "Mem [" + RenderBuffer::formatBytes(data.memInfo.usedBytes) + "/" +
                          RenderBuffer::formatBytes(data.memInfo.totalBytes) + "]";
        buf.drawProgressBar(curRow, leftCol, colWidth, data.memInfo.memUsagePercent, memLabel,
                            Color::FG_BRIGHT_MAGENTA, Color::FG_BRIGHT_CYAN, Color::FG_CYAN);

        string swapLabel = "Swp [" + RenderBuffer::formatBytes(data.memInfo.swapUsedBytes) + "/" +
                           RenderBuffer::formatBytes(data.memInfo.swapTotalBytes) + "]";
        buf.drawProgressBar(curRow, rightCol, colWidth, data.memInfo.swapUsagePercent, swapLabel,
                            Color::FG_BRIGHT_RED, Color::FG_BRIGHT_YELLOW, Color::FG_GREEN);
        curRow++;

        // Sparkline & I/O Dashboard Line
        size_t graphWidth = static_cast<size_t>(max(10, colWidth / 2 - 8));
        string cpuSpark = cpuGraph.renderBrailleLine(graphWidth, 0.0, 100.0, Color::FG_BRIGHT_CYAN);
        string memSpark = memGraph.renderBrailleLine(graphWidth, 0.0, 100.0, Color::FG_BRIGHT_MAGENTA);

        ostringstream leftInfo;
        leftInfo << Color::BOLD << "CPU Trend: " << Color::RESET << cpuSpark
                 << "  " << Color::FG_YELLOW << Color::BOLD << "Disk R/W: " << Color::RESET
                 << RenderBuffer::formatRate(data.diskInfo.totalReadBytesSec) << " / "
                 << RenderBuffer::formatRate(data.diskInfo.totalWriteBytesSec);

        ostringstream rightInfo;
        rightInfo << Color::BOLD << "Mem Trend: " << Color::RESET << memSpark
                  << "  " << Color::FG_GREEN << Color::BOLD << "Net RX/TX: " << Color::RESET
                  << RenderBuffer::formatRate(data.netInfo.totalRxBytesSec) << " / "
                  << RenderBuffer::formatRate(data.netInfo.totalTxBytesSec);

        buf.writeText(curRow, leftCol, leftInfo.str());
        buf.writeText(curRow++, rightCol, rightInfo.str());

    } else {
        int colWidth = cols - 2;
        buf.drawProgressBar(curRow++, 1, colWidth, data.totalCpuUsage, "CPU Avg");
        string memLabel = "Mem [" + RenderBuffer::formatBytes(data.memInfo.usedBytes) + "/" +
                          RenderBuffer::formatBytes(data.memInfo.totalBytes) + "]";
        buf.drawProgressBar(curRow++, 1, colWidth, data.memInfo.memUsagePercent, memLabel);
        string swapLabel = "Swp [" + RenderBuffer::formatBytes(data.memInfo.swapUsedBytes) + "/" +
                           RenderBuffer::formatBytes(data.memInfo.swapTotalBytes) + "]";
        buf.drawProgressBar(curRow++, 1, colWidth, data.memInfo.swapUsagePercent, swapLabel);
    }

    // System summary line: Tasks, Load Average, Temp, Uptime
    ostringstream sumSS;
    sumSS << Color::FG_CYAN << Color::BOLD << "Tasks: " << Color::RESET
          << data.snapshot.taskCounts.total << " total, "
          << Color::FG_GREEN << data.snapshot.taskCounts.running << " running" << Color::RESET << ", "
          << data.snapshot.taskCounts.sleeping << " sleeping, "
          << Color::FG_RED << data.snapshot.taskCounts.zombie << " zombie" << Color::RESET << "  |  "
          << Color::FG_YELLOW << Color::BOLD << "Load: " << Color::RESET
          << fixed << setprecision(2) << data.cpuInfo.load1 << " " << data.cpuInfo.load5 << " " << data.cpuInfo.load15;

    if (data.sensorInfo.isAvailable && data.sensorInfo.cpuTempC > 0) {
        string tempColor = (data.sensorInfo.cpuTempC > 80.0) ? Color::FG_BRIGHT_RED :
                           ((data.sensorInfo.cpuTempC > 65.0) ? Color::FG_BRIGHT_YELLOW : Color::FG_BRIGHT_GREEN);
        sumSS << "  |  " << Color::BOLD << "Temp: " << tempColor << fixed << setprecision(1) << data.sensorInfo.cpuTempC << "°C" << Color::RESET;
    }

    sumSS << "  |  " << Color::FG_MAGENTA << Color::BOLD << "Uptime: " << Color::RESET
          << RenderBuffer::formatTime(data.cpuInfo.uptimeSeconds);

    buf.writeText(curRow++, 1, sumSS.str());
}

void App::drawProcessTable(RenderBuffer& buf, const vector<Process>& procs, int startRow, int maxRows) {
    int cols = buf.getCols();
    string sortArrow = sortAscending ? "▲" : "▼";

    ostringstream hdr;
    hdr << Color::BG_CYAN << Color::FG_BLACK << Color::BOLD;

    auto getHdrCol = [&](const string& name, SortField f, size_t width) {
        string s = name;
        if (currentSort == f) s += sortArrow;
        return RenderBuffer::truncateOrPad(s, width, true);
    };

    hdr << getHdrCol("PID", SortField::PID, 7)
        << getHdrCol("USER", SortField::USER, 10)
        << getHdrCol("PRI", SortField::PID, 5)
        << getHdrCol("NI", SortField::PID, 4)
        << getHdrCol("VIRT", SortField::VIRT, 8)
        << getHdrCol("RES", SortField::RES, 8)
        << getHdrCol("SHR", SortField::RES, 8)
        << "S "
        << getHdrCol("CPU%", SortField::CPU, 7)
        << getHdrCol("MEM%", SortField::MEM, 7)
        << getHdrCol("TIME+", SortField::TIME, 10)
        << "Command";

    string hdrStr = hdr.str();
    buf.fillRow(startRow, ' ', Color::BG_CYAN);
    buf.writeText(startRow, 1, hdrStr, Color::BG_CYAN + Color::FG_BLACK + Color::BOLD);

    int printRow = startRow + 1;
    for (int i = 0; i < maxRows; ++i) {
        int procIdx = scrollOffset + i;
        if (procIdx >= static_cast<int>(procs.size())) {
            buf.fillRow(printRow++, ' ');
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

        buf.fillRow(printRow, ' ', rowStyle);

        ostringstream rowSS;
        rowSS << RenderBuffer::truncateOrPad(to_string(p.pid), 7, true)
              << RenderBuffer::truncateOrPad(p.user, 10, true)
              << RenderBuffer::truncateOrPad(to_string(p.priority), 5, true)
              << RenderBuffer::truncateOrPad(to_string(p.nice), 4, true)
              << RenderBuffer::truncateOrPad(RenderBuffer::formatBytes(p.virt_bytes), 8, true)
              << RenderBuffer::truncateOrPad(RenderBuffer::formatBytes(p.res_bytes), 8, true)
              << RenderBuffer::truncateOrPad(RenderBuffer::formatBytes(p.shr_bytes), 8, true)
              << p.state << " ";

        ostringstream cpuSS;
        cpuSS << fixed << setprecision(1) << p.cpu_usage;
        string cpuStr = RenderBuffer::truncateOrPad(cpuSS.str(), 7, true);
        if (!isSelected && p.cpu_usage > 50.0) {
            rowSS << Color::FG_BRIGHT_RED << Color::BOLD << cpuStr << Color::RESET;
            if (!rowStyle.empty()) rowSS << rowStyle;
        } else if (!isSelected && p.cpu_usage > 20.0) {
            rowSS << Color::FG_BRIGHT_YELLOW << cpuStr << Color::RESET;
            if (!rowStyle.empty()) rowSS << rowStyle;
        } else {
            rowSS << cpuStr;
        }

        ostringstream memSS;
        memSS << fixed << setprecision(1) << p.mem_usage;
        rowSS << RenderBuffer::truncateOrPad(memSS.str(), 7, true);

        rowSS << RenderBuffer::truncateOrPad(RenderBuffer::formatTime(p.cpu_time_seconds), 10, true);

        int usedCols = 7 + 10 + 5 + 4 + 8 + 8 + 8 + 2 + 7 + 7 + 10;
        int remainingCols = cols - usedCols;
        if (remainingCols > 0) {
            rowSS << RenderBuffer::truncateOrPad(p.cmdline, remainingCols, true);
        }

        buf.writeText(printRow++, 1, rowSS.str(), rowStyle);
    }
}

void App::drawFooter(RenderBuffer& buf, int row) {
    string fStyle = Color::BG_BLACK + Color::FG_WHITE;
    buf.fillRow(row, ' ', fStyle);

    ostringstream ss;
    auto addKey = [&](const string& k, const string& desc) {
        ss << Color::BG_CYAN << Color::FG_BLACK << Color::BOLD << " " << k << " " << Color::RESET
           << Color::BG_BLACK << Color::FG_WHITE << desc << " ";
    };

    addKey("F1", "Help");
    addKey("F3", "Search");
    addKey("F6", "Sort");
    addKey("F9", "Kill");
    addKey("Space", paused.load() ? "Resume" : "Pause");
    addKey("q", "Quit");

    buf.writeText(row, 1, ss.str(), fStyle);
}

void App::drawModals(RenderBuffer& buf) {
    if (activeModal == ModalType::NONE) return;

    int cols = buf.getCols();
    int rows = buf.getRows();

    if (activeModal == ModalType::HELP) {
        int width = min(64, cols - 4);
        int height = min(20, rows - 4);
        int startX = (cols - width) / 2;
        int startY = (rows - height) / 2;

        buf.drawBox(startY, startX, height, width, " Help & Keyboard Shortcuts ", Color::BG_BLACK + Color::FG_CYAN + Color::BOLD);
        for (int r = startY + 1; r < startY + height - 1; ++r) {
            buf.writeText(r, startX + 1, string(width - 2, ' '), Color::BG_BLACK);
        }

        int r = startY + 1;
        auto addHelp = [&](const string& key, const string& desc) {
            if (r < startY + height - 1) {
                buf.writeText(r, startX + 3, key, Color::BG_BLACK + Color::FG_YELLOW + Color::BOLD);
                buf.writeText(r++, startX + 18, desc, Color::BG_BLACK + Color::FG_WHITE);
            }
        };

        addHelp("Up / Down, j/k", "Navigate process list");
        addHelp("PgUp / PgDn", "Scroll 15 processes");
        addHelp("Home / End, g/G", "Jump to top / bottom");
        addHelp("/ or F3", "Live substring search / filter");
        addHelp("c / m / p / t", "Sort by CPU%, MEM%, PID, TIME+");
        addHelp("u / n", "Sort by USER, Command Name");
        addHelp("r / I", "Invert sort direction (asc/desc)");
        addHelp("F6", "Open interactive sort menu");
        addHelp("F9 or k", "Send signal (SIGTERM/KILL) to process");
        addHelp("Space", "Pause / Resume live monitoring");
        addHelp("q / F10", "Quit process monitor");
        addHelp("Esc / Enter", "Close this help dialog");
    } else if (activeModal == ModalType::SORT_SELECT) {
        int width = 36;
        int height = 12;
        int startX = (cols - width) / 2;
        int startY = (rows - height) / 2;

        buf.drawBox(startY, startX, height, width, " Select Sort Field ", Color::BG_BLACK + Color::FG_CYAN + Color::BOLD);
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

        buf.drawBox(startY, startX, height, width, " Send Signal to Process ", Color::BG_BLACK + Color::FG_RED + Color::BOLD);
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
