#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include "analytics.h"
#include "csv_handler.h"
#include "httplib.h"
#include "server_config.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::string normalizeMetal(std::string metal) {
    std::transform(metal.begin(), metal.end(), metal.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return metal;
}

bool isValidDate(const std::string& date) {
    static const std::regex pattern(R"(^\d{4}-\d{2}-\d{2}$)");
    return std::regex_match(date, pattern);
}

bool isValidMetal(const std::string& metal) {
    return metal == "gold" || metal == "silver";
}

void addCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

void sendJson(httplib::Response& res, const json& payload, int status = 200) {
    res.status = status;
    addCorsHeaders(res);
    res.set_content(payload.dump(2), "application/json");
}

void sendError(httplib::Response& res, int status, const std::string& message) {
    sendJson(res, json{{"error", message}}, status);
}

bool parsePositiveInt(const std::string& value, int& output) {
    try {
        size_t processed = 0;
        const int parsed = std::stoi(value, &processed);
        if (processed != value.size() || parsed <= 0) {
            return false;
        }
        output = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool validateRecord(const json& payload, MetalData& record, std::string& error) {
    if (!payload.contains("date") || !payload.contains("metal") || !payload.contains("price")) {
        error = "Missing required fields: date, metal, price.";
        return false;
    }
    if (!payload["date"].is_string() || !payload["metal"].is_string() ||
        !(payload["price"].is_number_float() || payload["price"].is_number_integer())) {
        error = "Invalid input types.";
        return false;
    }

    record.date = payload["date"].get<std::string>();
    record.metal = normalizeMetal(payload["metal"].get<std::string>());
    record.price = payload["price"].get<double>();

    if (!isValidDate(record.date)) {
        error = "Date must be in YYYY-MM-DD format.";
        return false;
    }
    if (!isValidMetal(record.metal)) {
        error = "Metal must be either gold or silver.";
        return false;
    }
    if (record.price < 0.0) {
        error = "Price must be non-negative.";
        return false;
    }

    return true;
}

json buildAnalyticsPayload(const std::vector<MetalData>& records, const std::string& metal) {
    return json{
        {"metal", metal},
        {"current_price", getLatestPrice(records, metal)},
        {"trend", detectTrend(records, metal)},
        {"difference", calculateDifference(records, metal)}
    };
}
}  // namespace

void registerRoutes(httplib::Server& server, CSVHandler& csv_handler) {
    server.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        addCorsHeaders(res);
        res.status = 200;
    });

    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json{
            {"message", "Metal price tracker backend is running."},
            {"frontend", "Open frontend/index.html or frontend/dashboard.html in your browser."},
            {"api", json::array({"/get-data?page=1&limit=10", "/add-data", "/update-data", "/delete-data", "/analytics", "/download-csv"})}
        });
    });

    server.Get("/get-data", [&](const httplib::Request& req, httplib::Response& res) {
        int page = server_config::DEFAULT_PAGE;
        int limit = server_config::DEFAULT_LIMIT;

        if (req.has_param("page") && !parsePositiveInt(req.get_param_value("page"), page)) {
            sendError(res, 400, "Invalid query param: page must be a positive integer.");
            return;
        }
        if (req.has_param("limit") && !parsePositiveInt(req.get_param_value("limit"), limit)) {
            sendError(res, 400, "Invalid query param: limit must be a positive integer.");
            return;
        }
        if (limit > server_config::MAX_LIMIT) {
            sendError(res, 400, "Limit exceeds maximum allowed value.");
            return;
        }

        const auto& all_records = csv_handler.getData();
        if (all_records.empty()) {
            sendError(res, 404, "Dataset is empty.");
            return;
        }

        const auto paginated = csv_handler.getPaginatedData(page, limit);
        json items = json::array();
        for (const auto& row : paginated) {
            items.push_back({
                {"date", row.date},
                {"metal", row.metal},
                {"price", row.price}
            });
        }

        const int total = static_cast<int>(all_records.size());
        const int total_pages = static_cast<int>(std::ceil(static_cast<double>(total) / limit));

        sendJson(res, json{
            {"page", page},
            {"limit", limit},
            {"total", total},
            {"total_pages", total_pages},
            {"data", items}
        });
    });

    server.Post("/add-data", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto payload = json::parse(req.body);
            MetalData record;
            std::string error;
            if (!validateRecord(payload, record, error)) {
                sendError(res, 400, error);
                return;
            }

            if (!csv_handler.appendRow(record, error)) {
                sendError(res, 400, error);
                return;
            }

            sendJson(res, json{{"message", "Record added successfully."}}, 201);
        } catch (...) {
            sendError(res, 400, "Invalid JSON input.");
        }
    });

    server.Put("/update-data", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto payload = json::parse(req.body);
            MetalData record;
            std::string error;
            if (!validateRecord(payload, record, error)) {
                sendError(res, 400, error);
                return;
            }

            if (!csv_handler.updateRow(record.date, record.metal, record.price, error)) {
                sendError(res, 404, error);
                return;
            }

            sendJson(res, json{{"message", "Record updated successfully."}});
        } catch (...) {
            sendError(res, 400, "Invalid JSON input.");
        }
    });

    server.Delete("/delete-data", [&](const httplib::Request& req, httplib::Response& res) {
        std::string date;
        std::string metal;

        if (req.has_param("date") && req.has_param("metal")) {
            date = req.get_param_value("date");
            metal = normalizeMetal(req.get_param_value("metal"));
        } else {
            try {
                const auto payload = json::parse(req.body);
                if (!payload.contains("date") || !payload.contains("metal") ||
                    !payload["date"].is_string() || !payload["metal"].is_string()) {
                    sendError(res, 400, "Missing required fields: date, metal.");
                    return;
                }
                date = payload["date"].get<std::string>();
                metal = normalizeMetal(payload["metal"].get<std::string>());
            } catch (...) {
                sendError(res, 400, "Invalid input for delete request.");
                return;
            }
        }

        if (!isValidDate(date) || !isValidMetal(metal)) {
            sendError(res, 400, "Invalid date or metal value.");
            return;
        }

        std::string error;
        if (!csv_handler.deleteRow(date, metal, error)) {
            sendError(res, 404, error);
            return;
        }

        sendJson(res, json{{"message", "Record deleted successfully."}});
    });

    server.Get("/analytics", [&](const httplib::Request& req, httplib::Response& res) {
        const auto& records = csv_handler.getData();
        if (records.empty()) {
            sendError(res, 404, "Dataset is empty.");
            return;
        }

        if (req.has_param("metal")) {
            const std::string metal = normalizeMetal(req.get_param_value("metal"));
            if (!isValidMetal(metal)) {
                sendError(res, 400, "Metal must be either gold or silver.");
                return;
            }
            sendJson(res, buildAnalyticsPayload(records, metal));
            return;
        }

        sendJson(res, json{
            {"gold", buildAnalyticsPayload(records, "gold")},
            {"silver", buildAnalyticsPayload(records, "silver")}
        });
    });

    server.Get("/download-csv", [&](const httplib::Request&, httplib::Response& res) {
        std::ifstream file(server_config::DATASET_PATH, std::ios::binary);
        if (!file.is_open()) {
            sendError(res, 404, "File not found.");
            return;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        addCorsHeaders(res);
        res.set_header("Content-Disposition", "attachment; filename=\"dataset.csv\"");
        res.set_content(buffer.str(), "text/csv");
    });
}
