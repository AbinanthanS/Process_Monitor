#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <string>
#include <deque>

class SparklineGraph {
public:
    explicit SparklineGraph(size_t maxHistory = 60);

    void addSample(double value);
    void setMaxHistory(size_t maxHistory);

    // Renders a single-line Braille sparkline graph of given visual column width
    std::string renderBrailleLine(size_t width, double minVal = 0.0, double maxVal = 100.0, const std::string& color = "") const;

    // Renders a multi-line Braille graph (height rows x width columns)
    std::vector<std::string> renderBrailleMatrix(size_t height, size_t width, double minVal = 0.0, double maxVal = 100.0, const std::string& color = "") const;

    const std::deque<double>& getHistory() const { return history; }
    double getLatest() const { return history.empty() ? 0.0 : history.back(); }
    double getAverage() const;

private:
    size_t maxCapacity;
    std::deque<double> history;

    // Generates a UTF-8 Braille character for 2 columns with heights 0..4
    static std::string getBrailleChar(int leftDotHeight, int rightDotHeight);
};

#endif // GRAPH_H
