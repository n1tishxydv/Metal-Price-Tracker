#include "analytics.h"

#include <algorithm>
#include <vector>

namespace {
std::vector<MetalData> collectMetalRecords(const std::vector<MetalData>& records, const std::string& metal) {
    std::vector<MetalData> filtered;
    for (const auto& row : records) {
        if (row.metal == metal) {
            filtered.push_back(row);
        }
    }
    std::sort(filtered.begin(), filtered.end(), [](const MetalData& left, const MetalData& right) {
        return left.date < right.date;
    });
    return filtered;
}
}  // namespace

double getLatestPrice(const std::vector<MetalData>& records, const std::string& metal) {
    const auto filtered = collectMetalRecords(records, metal);
    if (filtered.empty()) {
        return 0.0;
    }
    return filtered.back().price;
}

double calculateDifference(const std::vector<MetalData>& records, const std::string& metal) {
    const auto filtered = collectMetalRecords(records, metal);
    if (filtered.size() < 2) {
        return 0.0;
    }
    return filtered.back().price - filtered[filtered.size() - 2].price;
}

std::string detectTrend(const std::vector<MetalData>& records, const std::string& metal) {
    const auto filtered = collectMetalRecords(records, metal);
    if (filtered.size() < 2) {
        return "Stable";
    }

    const double current = filtered.back().price;
    const double previous = filtered[filtered.size() - 2].price;

    if (current > previous) {
        return "Rising";
    }
    if (current < previous) {
        return "Falling";
    }
    return "Stable";
}
