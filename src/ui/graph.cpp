#include "ui/graph.h"
#include "ui/render_buffer.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstdint>

using namespace std;

SparklineGraph::SparklineGraph(size_t maxHistory) : maxCapacity(maxHistory) {}

void SparklineGraph::setMaxHistory(size_t maxHistory) {
    maxCapacity = maxHistory;
    while (history.size() > maxCapacity) {
        history.pop_front();
    }
}

void SparklineGraph::addSample(double value) {
    history.push_back(value);
    if (history.size() > maxCapacity) {
        history.pop_front();
    }
}

double SparklineGraph::getAverage() const {
    if (history.empty()) return 0.0;
    double sum = accumulate(history.begin(), history.end(), 0.0);
    return sum / history.size();
}

double SparklineGraph::getMax() const {
    if (history.empty()) return 0.0;
    return *max_element(history.begin(), history.end());
}

double SparklineGraph::getMin() const {
    if (history.empty()) return 0.0;
    return *min_element(history.begin(), history.end());
}

string SparklineGraph::getBrailleChar(int leftDotHeight, int rightDotHeight, bool /*filled*/) {
    leftDotHeight = clamp(leftDotHeight, 0, 4);
    rightDotHeight = clamp(rightDotHeight, 0, 4);

    static const uint8_t leftMasks[] = {0x00, 0x40, 0x44, 0x46, 0x47};
    static const uint8_t rightMasks[] = {0x00, 0x80, 0xA0, 0xB0, 0xB8};

    uint8_t byteOffset = leftMasks[leftDotHeight] | rightMasks[rightDotHeight];
    uint32_t codepoint = 0x2800 + byteOffset;

    string utf8;
    utf8.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    return utf8;
}

string SparklineGraph::renderBrailleLine(size_t width, double minVal, double maxVal, const string& color) const {
    if (width == 0) return "";
    size_t samplesNeeded = width * 2;

    vector<double> samples(samplesNeeded, minVal);
    if (!history.empty()) {
        size_t available = min(history.size(), samplesNeeded);
        size_t startIdx = samplesNeeded - available;
        for (size_t i = 0; i < available; ++i) {
            samples[startIdx + i] = history[history.size() - available + i];
        }
    }

    double range = (maxVal > minVal) ? (maxVal - minVal) : 1.0;

    string line;
    if (!color.empty()) line += color;

    for (size_t i = 0; i < width; ++i) {
        double leftVal = samples[i * 2];
        double rightVal = samples[i * 2 + 1];

        int leftH = static_cast<int>(round(((leftVal - minVal) / range) * 4.0));
        int rightH = static_cast<int>(round(((rightVal - minVal) / range) * 4.0));

        line += getBrailleChar(leftH, rightH);
    }

    if (!color.empty()) line += Color::RESET;
    return line;
}

vector<string> SparklineGraph::renderBrailleMatrix(size_t height, size_t width, double minVal, double maxVal,
                                                   const string& startColor, const string& endColor,
                                                   bool fillArea) const {
    vector<string> lines(height);
    if (height == 0 || width == 0) return lines;

    size_t samplesNeeded = width * 2;
    vector<double> samples(samplesNeeded, minVal);
    if (!history.empty()) {
        size_t available = min(history.size(), samplesNeeded);
        size_t startIdx = samplesNeeded - available;
        for (size_t i = 0; i < available; ++i) {
            samples[startIdx + i] = history[history.size() - available + i];
        }
    }

    double range = (maxVal > minVal) ? (maxVal - minVal) : 1.0;
    int totalDotsY = static_cast<int>(height * 4);

    for (size_t r = 0; r < height; ++r) {
        string line;
        string lineCol = startColor;
        if (!startColor.empty() && !endColor.empty()) {
            double factor = (height > 1) ? static_cast<double>(r) / (height - 1) : 0.0;
            lineCol = Color::gradient(255, 60, 60, 60, 220, 255, 1.0 - factor);
        }

        if (!lineCol.empty()) line += lineCol;

        int rowBottomDot = static_cast<int>((height - 1 - r) * 4);

        for (size_t c = 0; c < width; ++c) {
            double leftVal = samples[c * 2];
            double rightVal = samples[c * 2 + 1];

            int leftTotalH = static_cast<int>(round(((leftVal - minVal) / range) * totalDotsY));
            int rightTotalH = static_cast<int>(round(((rightVal - minVal) / range) * totalDotsY));

            int leftDotH = 0;
            int rightDotH = 0;

            if (fillArea) {
                if (leftTotalH >= rowBottomDot + 4) leftDotH = 4;
                else if (leftTotalH <= rowBottomDot) leftDotH = 0;
                else leftDotH = leftTotalH - rowBottomDot;

                if (rightTotalH >= rowBottomDot + 4) rightDotH = 4;
                else if (rightTotalH <= rowBottomDot) rightDotH = 0;
                else rightDotH = rightTotalH - rowBottomDot;
            } else {
                leftDotH = clamp(leftTotalH - rowBottomDot, 0, 4);
                rightDotH = clamp(rightTotalH - rowBottomDot, 0, 4);
            }

            line += getBrailleChar(leftDotH, rightDotH, fillArea);
        }

        if (!lineCol.empty()) line += Color::RESET;
        lines[r] = line;
    }

    return lines;
}
