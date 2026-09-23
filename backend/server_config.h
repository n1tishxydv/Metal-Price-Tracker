#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>

namespace server_config {
const char* const HOST = "localhost";
const int PORT = 8080;
const std::string DATASET_PATH = "data/dataset.csv";
const std::string DATA_DIRECTORY = "data";
const int DEFAULT_PAGE = 1;
const int DEFAULT_LIMIT = 10;
const int MAX_LIMIT = 50;
}  // namespace server_config

#endif
