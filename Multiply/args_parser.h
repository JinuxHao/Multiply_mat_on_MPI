#pragma once

#include <cstring>
#include <iostream>

inline bool Parse_arg(int argc, char* argv[], const char** pathA, const char** pathB, const char** pathC)
{
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc || argv[i + 1][0] == '\0' || std::strncmp(argv[i + 1], "--", 2) == 0) {
            std::cerr << "error: missing value for " << argv[i] << '\n';
            return false;
        }

        if (std::strcmp(argv[i], "--pathA") == 0 && *pathA == nullptr)
            *pathA = argv[i + 1];
        else if (std::strcmp(argv[i], "--pathB") == 0 && *pathB == nullptr)
            *pathB = argv[i + 1];
        else if (std::strcmp(argv[i], "--pathC") == 0 && *pathC == nullptr)
            *pathC = argv[i + 1];
        else {
            std::cerr << "error: unknown or duplicate argument " << argv[i] << '\n';
            return false;
        }
    }

    if (*pathA == nullptr || *pathB == nullptr) {
        std::cerr << "error: --pathA and --pathB are required\n";
        return false;
    }
    if (*pathC == nullptr)
        *pathC = "C.txt";

    if (std::strcmp(*pathC, *pathA) == 0 || std::strcmp(*pathC, *pathB) == 0) {
        std::cerr << "error: output path must differ from input paths\n";
        return false;
    }
    return true;
}
