#ifndef APP_H
#define APP_H

#include "ui/terminal.h"
#include "ui/render_buffer.h"
#include "ui/graph.h"
#include "collectors/cpu.h"
#include "collectors/memory.h"
#include "collectors/process.h"
#include "collectors/disk.h"
#include "collectors/net.h"
#include "collectors/sensors.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

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
    KILL_CONFIRM,
    MODULE_SELECT
};

enum class LayoutPreset {
    FULL = 0,       // All modules (CPU, Mem, Disk, Net, Proc)
    RESOURCES,     // CPU, Mem, Disk, Net
    PROCESSES,     // Only Processes
    IO_FOCUS,      // Disk, Net, Proc
    MINIMAL,       // CPU + Mem
    COUNT
};

struct ModuleConfig {
    bool cpu = true;
    bool mem = true;
    bool disk = true;
    bool net = true;
    bool proc = true;

    int activeCount() const {
        return (cpu ? 1 : 0) + (mem ? 1 : 0) + (disk ? 1 : 0) + (net ? 1 : 0) + (proc ? 1 : 0);
    }
};

struct LayoutBoxes {
    Rect topBar;
    Rect cpu;
    Rect mem;
    Rect disk;
    Rect net;
    Rect proc;
    Rect searchBar;
    Rect footer;
};

struct AppData {
    SystemCPUInfo cpuInfo;
    std::vector<double> coreUsages;
    double totalCpuUsage = 0.0;
    MemoryInfo memInfo;
    ProcessSnapshot snapshot;
    SystemDiskInfo diskInfo;
    SystemNetInfo netInfo;
    SensorInfo sensorInfo;
};

class App {
public:
    App();
    ~App();

    void run();

    // Module configuration
    void setModuleEnabled(const std::string& modName, bool enabled);
    void applyPreset(LayoutPreset preset);
    void setRefreshInterval(int ms) { refreshIntervalMs.store(ms); }

private:
    void collectorLoop();
    void processInput(const KeyEvent& evt);
    void render();

    LayoutBoxes computeLayout(int rows, int cols);

    // Module renderers
    void renderTopBar(RenderBuffer& buf, const Rect& rect, const AppData& data);
    void renderCpuPanel(RenderBuffer& buf, const Rect& rect, const AppData& data);
    void renderMemPanel(RenderBuffer& buf, const Rect& rect, const AppData& data);
    void renderDiskPanel(RenderBuffer& buf, const Rect& rect, const AppData& data);
    void renderNetPanel(RenderBuffer& buf, const Rect& rect, const AppData& data);
    void renderProcPanel(RenderBuffer& buf, const Rect& rect, const std::vector<Process>& procs);
    void renderFooter(RenderBuffer& buf, const Rect& rect);
    void renderModals(RenderBuffer& buf);

    void applySortAndFilter(const std::vector<Process>& source, std::vector<Process>& dest);
    void sendSignalToSelected(int signalNum);
    void cycleSelectedInterface(int direction);

    Terminal& term;
    std::atomic<bool> running{true};
    std::atomic<bool> paused{false};
    std::atomic<int> refreshIntervalMs{1000};

    std::thread collectorThread;
    std::mutex dataMutex;
    AppData appData;

    // Sparkline & Braille Graphs
    SparklineGraph cpuGraph{120};
    SparklineGraph memGraph{120};
    SparklineGraph diskReadGraph{120};
    SparklineGraph diskWriteGraph{120};
    SparklineGraph netRxGraph{120};
    SparklineGraph netTxGraph{120};

    // Active modules & presets
    ModuleConfig modules;
    LayoutPreset currentPreset = LayoutPreset::FULL;
    int selectedNetInterfaceIdx = 0;

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
