#include "csv_handler.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

#include "server_config.h"

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}  // namespace

CSVHandler::CSVHandler(std::string file_path) : file_path_(std::move(file_path)) {}

bool CSVHandler::loadCSV(std::string& error) {
    if (!loadPrimaryDataset(error)) {
        return false;
    }

    const std::vector<MetalData>::size_type before_import = data_.size();
    if (!importAdditionalCSVFiles(error)) {
        return false;
    }

    sortRecords(data_);
    if (data_.size() != before_import && !saveCSV(error)) {
        return false;
    }

    error.clear();
    return true;
}

bool CSVHandler::loadPrimaryDataset(std::string& error) {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        error = "File not found: " + file_path_;
        return false;
    }

    std::vector<MetalData> loaded_records;
    std::string line;

    if (!std::getline(file, line)) {
        error = "Dataset is empty.";
        return false;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const auto columns = parseCSVRow(line);
        if (columns.size() < 3) {
            error = "Invalid CSV row format in dataset.csv.";
            return false;
        }

        double price = 0.0;
        if (!parsePriceText(columns[2], price)) {
            error = "Invalid price value in dataset.csv.";
            return false;
        }

        loaded_records.push_back({trim(columns[0]), toLower(trim(columns[1])), price});
    }

    data_ = std::move(loaded_records);
    sortRecords(data_);
    error.clear();
    return true;
}

bool CSVHandler::importAdditionalCSVFiles(std::string& error) {
    std::vector<std::string> source_files;
    source_files.push_back(server_config::DATA_DIRECTORY + "/Gold Futures Historical Data (23.01.24-22.11.24).csv");
    source_files.push_back(server_config::DATA_DIRECTORY + "/Silver Futures Historical Data (23.01.24-22.11.24).csv");
    source_files.push_back(server_config::DATA_DIRECTORY + "/gold prices.csv");
    source_files.push_back(server_config::DATA_DIRECTORY + "/silver prices.csv");
    source_files.push_back(server_config::DATA_DIRECTORY + "/gold.csv");

    for (std::vector<std::string>::size_type index = 0; index < source_files.size(); ++index) {
        const std::string& path = source_files[index];
        if (path == file_path_) {
            continue;
        }

        std::ifstream probe(path.c_str());
        if (!probe.is_open()) {
            continue;
        }
        probe.close();

        if (path.find("Gold Futures Historical Data") != std::string::npos) {
            if (!importMarketCSV(path, "gold", data_, error)) {
                return false;
            }
            continue;
        }

        if (path.find("Silver Futures Historical Data") != std::string::npos) {
            if (!importMarketCSV(path, "silver", data_, error)) {
                return false;
            }
            continue;
        }

        if (path.find("gold prices.csv") != std::string::npos) {
            if (!importMarketCSV(path, "gold", data_, error)) {
                return false;
            }
            continue;
        }

        if (path.find("silver prices.csv") != std::string::npos) {
            if (!importMarketCSV(path, "silver", data_, error)) {
                return false;
            }
            continue;
        }

        if (path.find("gold.csv") != std::string::npos) {
            if (!importWideGoldCSV(path, data_, error)) {
                return false;
            }
        }
    }

    error.clear();
    return true;
}

bool CSVHandler::importNormalizedCSV(const std::string& path, std::vector<MetalData>& records, std::string& error) const {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        error = "Unable to open file: " + path;
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        error = "CSV file is empty: " + path;
        return false;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> columns = parseCSVRow(line);
        if (columns.size() < 3) {
            continue;
        }

        double price = 0.0;
        if (!parsePriceText(columns[2], price)) {
            continue;
        }

        MetalData record;
        record.date = normalizeDate(trim(columns[0]));
        record.metal = toLower(trim(columns[1]));
        record.price = price;

        if (!record.date.empty() && (record.metal == "gold" || record.metal == "silver") &&
            !isDuplicateRecord(records, record)) {
            records.push_back(record);
        }
    }

    error.clear();
    return true;
}

bool CSVHandler::importMarketCSV(const std::string& path, const std::string& metal, std::vector<MetalData>& records, std::string& error) const {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        error = "Unable to open file: " + path;
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        error = "CSV file is empty: " + path;
        return false;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> columns = parseCSVRow(line);
        if (columns.size() < 2) {
            continue;
        }

        double price = 0.0;
        if (!parsePriceText(columns[1], price)) {
            continue;
        }

        MetalData record;
        record.date = normalizeDate(trim(columns[0]));
        record.metal = metal;
        record.price = price;

        if (!record.date.empty() && !isDuplicateRecord(records, record)) {
            records.push_back(record);
        }
    }

    error.clear();
    return true;
}

bool CSVHandler::importWideGoldCSV(const std::string& path, std::vector<MetalData>& records, std::string& error) const {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        error = "Unable to open file: " + path;
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        error = "CSV file is empty: " + path;
        return false;
    }

    const std::vector<std::string> headers = parseCSVRow(line);
    int date_index = -1;
    int inr_index = -1;

    for (std::vector<std::string>::size_type i = 0; i < headers.size(); ++i) {
        const std::string header = trim(headers[i]);
        if (header == "Date") {
            date_index = static_cast<int>(i);
        } else if (header == "INR") {
            inr_index = static_cast<int>(i);
        }
    }

    if (date_index < 0 || inr_index < 0) {
        error = "Required columns not found in gold.csv.";
        return false;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> columns = parseCSVRow(line);
        if (static_cast<int>(columns.size()) <= std::max(date_index, inr_index)) {
            continue;
        }

        double price = 0.0;
        if (!parsePriceText(columns[inr_index], price)) {
            continue;
        }

        MetalData record;
        record.date = normalizeDate(trim(columns[date_index]));
        record.metal = "gold";
        record.price = price;

        if (!record.date.empty() && !isDuplicateRecord(records, record)) {
            records.push_back(record);
        }
    }

    error.clear();
    return true;
}

bool CSVHandler::saveCSV(std::string& error) const {
    const std::string temp_path = file_path_ + ".tmp";
    std::ofstream file(temp_path, std::ios::trunc);
    if (!file.is_open()) {
        error = "Unable to write dataset.";
        return false;
    }

    file << "date,metal,price\n";
    for (const auto& row : data_) {
        file << row.date << ',' << row.metal << ','
             << std::fixed << std::setprecision(2) << row.price << '\n';
    }

    file.close();
    if (!file) {
        error = "Failed while saving dataset.";
        return false;
    }

    std::remove(file_path_.c_str());
    if (std::rename(temp_path.c_str(), file_path_.c_str()) != 0) {
        error = "Unable to replace dataset file.";
        return false;
    }

    error.clear();
    return true;
}

bool CSVHandler::appendRow(const MetalData& record, std::string& error) {
    if (hasRecord(record.date, record.metal)) {
        error = "Record already exists for the given date and metal.";
        return false;
    }

    data_.push_back(record);
    sortRecords(data_);
    return saveCSV(error);
}

bool CSVHandler::updateRow(const std::string& date, const std::string& metal, double new_price, std::string& error) {
    auto it = std::find_if(data_.begin(), data_.end(), [&](const MetalData& row) {
        return row.date == date && row.metal == metal;
    });

    if (it == data_.end()) {
        error = "Record not found.";
        return false;
    }

    it->price = new_price;
    sortRecords(data_);
    return saveCSV(error);
}

bool CSVHandler::deleteRow(const std::string& date, const std::string& metal, std::string& error) {
    auto old_size = data_.size();
    data_.erase(
        std::remove_if(data_.begin(), data_.end(), [&](const MetalData& row) {
            return row.date == date && row.metal == metal;
        }),
        data_.end());

    if (data_.size() == old_size) {
        error = "Record not found.";
        return false;
    }

    return saveCSV(error);
}

const std::vector<MetalData>& CSVHandler::getData() const {
    return data_;
}

std::vector<MetalData> CSVHandler::getPaginatedData(int page, int limit) const {
    std::vector<MetalData> reversed(data_.rbegin(), data_.rend());
    const int start = (page - 1) * limit;
    if (start >= static_cast<int>(reversed.size())) {
        return {};
    }

    const int end = std::min(start + limit, static_cast<int>(reversed.size()));
    return std::vector<MetalData>(reversed.begin() + start, reversed.begin() + end);
}

bool CSVHandler::hasRecord(const std::string& date, const std::string& metal) const {
    return std::any_of(data_.begin(), data_.end(), [&](const MetalData& row) {
        return row.date == date && row.metal == metal;
    });
}

bool CSVHandler::isDuplicateRecord(const std::vector<MetalData>& records, const MetalData& candidate) const {
    return std::find_if(records.begin(), records.end(), [&](const MetalData& row) {
        return row.date == candidate.date && row.metal == candidate.metal;
    }) != records.end();
}

std::vector<std::string> CSVHandler::parseCSVRow(const std::string& line) {
    std::vector<std::string> columns;
    std::string current;
    bool inside_quotes = false;

    for (std::string::size_type i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inside_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inside_quotes = !inside_quotes;
            }
        } else if (ch == ',' && !inside_quotes) {
            columns.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    columns.push_back(current);
    return columns;
}

std::string CSVHandler::trim(const std::string& value) {
    std::string::size_type start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    std::string::size_type end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string CSVHandler::normalizeDate(const std::string& raw_date) {
    if (raw_date.size() == 10 && raw_date[4] == '-' && raw_date[7] == '-') {
        return raw_date;
    }

    if (raw_date.size() == 10 && raw_date[2] == '/' && raw_date[5] == '/') {
        const std::string month = raw_date.substr(0, 2);
        const std::string day = raw_date.substr(3, 2);
        const std::string year = raw_date.substr(6, 4);
        return year + "-" + month + "-" + day;
    }

    return "";
}

bool CSVHandler::parsePriceText(const std::string& raw_price, double& price) {
    std::string cleaned;
    for (std::string::size_type i = 0; i < raw_price.size(); ++i) {
        const char ch = raw_price[i];
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') {
            cleaned.push_back(ch);
        }
    }

    if (cleaned.empty()) {
        return false;
    }

    char* end_ptr = NULL;
    price = std::strtod(cleaned.c_str(), &end_ptr);
    return end_ptr != cleaned.c_str() && *end_ptr == '\0';
}

void CSVHandler::sortRecords(std::vector<MetalData>& records) {
    std::sort(records.begin(), records.end(), [](const MetalData& left, const MetalData& right) {
        if (left.date == right.date) {
            return left.metal < right.metal;
        }
        return left.date < right.date;
    });
}
