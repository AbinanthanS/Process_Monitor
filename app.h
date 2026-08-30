#ifndef APP_H
#define APP_H

#include "terminal.h"
#include "render_buffer.h"
#include "cpu.h"
#include "memory.h"
#include "process.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

enum class SortField {
    CPU,
    MEM,
    PID,
    USER,
    TIME,
    NAME,
    VIRT,
    RES
};

enum class ModalType {
    NONE,
    HELP,
    SORT_SELECT,
    KILL_CONFIRM
};

struct AppData {
    SystemCPUInfo cpuInfo;
    std::vector<double> coreUsages;
    double totalCpuUsage = 0.0;
    MemoryInfo memInfo;
    ProcessSnapshot snapshot;
};

class App {
public:
    App();
    ~App();

    void run();

private:
    void collectorLoop();
    void processInput(const KeyEvent& evt);
    void render();

    void drawHeader(RenderBuffer& buf, const AppData& data, int& curRow);
    void drawProcessTable(RenderBuffer& buf, const std::vector<Process>& procs, int startRow, int maxRows);
    void drawFooter(RenderBuffer& buf, int row);
    void drawModals(RenderBuffer& buf);

    void applySortAndFilter(const std::vector<Process>& source, std::vector<Process>& dest);
    void sendSignalToSelected(int signalNum);

    Terminal& term;
    std::atomic<bool> running{true};
    std::atomic<bool> paused{false};

    std::thread collectorThread;
    std::mutex dataMutex;
    AppData appData;

    // UI state
    int selectedIndex = 0;
    int scrollOffset = 0;
    SortField currentSort = SortField::CPU;
    bool sortAscending = false;

    bool searchMode = false;
    std::string searchQuery;

    ModalType activeModal = ModalType::NONE;
    int modalSelectedIndex = 0;

    std::string statusMessage;
    std::chrono::steady_clock::time_point statusMessageExpiry;
    void setStatus(const std::string& msg, int durationSeconds = 3);
};

#endif // APP_H
