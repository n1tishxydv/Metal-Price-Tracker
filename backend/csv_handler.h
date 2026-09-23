#ifndef CSV_HANDLER_H
#define CSV_HANDLER_H

#include <string>
#include <vector>

struct MetalData {
    std::string date;
    std::string metal;
    double price;
};

class CSVHandler {
public:
    explicit CSVHandler(std::string file_path);

    bool loadCSV(std::string& error);
    bool saveCSV(std::string& error) const;
    bool appendRow(const MetalData& record, std::string& error);
    bool updateRow(const std::string& date, const std::string& metal, double new_price, std::string& error);
    bool deleteRow(const std::string& date, const std::string& metal, std::string& error);

    const std::vector<MetalData>& getData() const;
    std::vector<MetalData> getPaginatedData(int page, int limit) const;
    bool hasRecord(const std::string& date, const std::string& metal) const;

private:
    std::string file_path_;
    std::vector<MetalData> data_;

    bool loadPrimaryDataset(std::string& error);
    bool importAdditionalCSVFiles(std::string& error);
    bool importNormalizedCSV(const std::string& path, std::vector<MetalData>& records, std::string& error) const;
    bool importMarketCSV(const std::string& path, const std::string& metal, std::vector<MetalData>& records, std::string& error) const;
    bool importWideGoldCSV(const std::string& path, std::vector<MetalData>& records, std::string& error) const;
    bool isDuplicateRecord(const std::vector<MetalData>& records, const MetalData& candidate) const;
    static std::vector<std::string> parseCSVRow(const std::string& line);
    static std::string trim(const std::string& value);
    static std::string normalizeDate(const std::string& raw_date);
    static bool parsePriceText(const std::string& raw_price, double& price);
    static void sortRecords(std::vector<MetalData>& records);
};

#endif
