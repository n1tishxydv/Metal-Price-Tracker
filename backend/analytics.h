#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <string>
#include <vector>

#include "csv_handler.h"

double getLatestPrice(const std::vector<MetalData>& records, const std::string& metal);
double calculateDifference(const std::vector<MetalData>& records, const std::string& metal);
std::string detectTrend(const std::vector<MetalData>& records, const std::string& metal);

#endif
