#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#endif

#include <iostream>

#include "csv_handler.h"
#include "httplib.h"
#include "server_config.h"

void registerRoutes(httplib::Server& server, CSVHandler& csv_handler);

// Single-translation-unit includes keep the requested compile flow simple.
#include "csv_handler.cpp"
#include "analytics.cpp"
#include "routes.cpp"

int main() {
    CSVHandler csv_handler(server_config::DATASET_PATH);
    std::string error;
    if (!csv_handler.loadCSV(error)) {
        std::cerr << "Startup error: " << error << std::endl;
        return 1;
    }

    httplib::Server server;
    registerRoutes(server, csv_handler);

    std::cout << "Server running at http://" << server_config::HOST << ':' << server_config::PORT << std::endl;
    if (!server.listen(server_config::HOST, server_config::PORT)) {
        std::cerr << "Failed to start server on port " << server_config::PORT << std::endl;
        return 1;
    }

    return 0;
}
