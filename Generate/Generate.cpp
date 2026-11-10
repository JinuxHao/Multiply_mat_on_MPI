#include "../myMat.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>

using namespace std;

static void Usage(ostream& out)
{
    out << "Usage: generate --pathA <file> --pathB <file> --heightA <rows> --widthA <cols>\n"
        << "                --heightB <rows> --widthB <cols> --min <int> --max <int>\n"
        << "       generate -h|--help\n"
        << "Example: ./generate --pathA A.bin --pathB B.bin --heightA 100 --widthA 200"
        << " --heightB 200 --widthB 100 --min -1 --max 1\n";
}

static bool ParseInt(const char* value, int& result)
{
    char* end = nullptr;
    errno = 0;
    const long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || errno == ERANGE ||
        parsed < numeric_limits<int>::min() || parsed > numeric_limits<int>::max()) {
        return false;
    }
    result = static_cast<int>(parsed);
    return true;
}

static bool GenerateAndWrite(int height, int width, int min, int max, const char* path,
                             const char* generateLabel, const char* writeLabel)
{
    Mat matrix = GenMat(height, width, static_cast<float>(min), static_cast<float>(max), generateLabel);
    if (matrix.data == nullptr)
        return false;
    const bool saved = WriteMat(path, matrix, writeLabel);
    ReleaseMat(&matrix);
    return saved;
}

int main(int argc, char* argv[])
{
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        Usage(cout);
        return 0;
    }

    enum Option { PathA, PathB, HeightA, WidthA, HeightB, WidthB, Min, Max, OptionCount };
    const char* const names[OptionCount] = {
        "--pathA", "--pathB", "--heightA", "--widthA", "--heightB", "--widthB", "--min", "--max"
    };
    const char* values[OptionCount] = {};
    for (int i = 1; i < argc; i += 2) {
        int option = 0;
        while (option < OptionCount && strcmp(argv[i], names[option]) != 0)
            ++option;
        if (option == OptionCount || values[option] != nullptr || i + 1 == argc ||
            argv[i + 1][0] == '\0' || strncmp(argv[i + 1], "--", 2) == 0) {
            cerr << "error: unknown or duplicate argument, or missing value for " << argv[i] << '\n';
            Usage(cerr);
            return 1;
        }
        values[option] = argv[i + 1];
    }

    for (int option = 0; option < OptionCount; ++option) {
        if (values[option] == nullptr) {
            cerr << "error: missing required option " << names[option] << '\n';
            Usage(cerr);
            return 1;
        }
    }

    int dimensionsAndBounds[OptionCount] = {};
    for (int option = HeightA; option < OptionCount; ++option) {
        if (!ParseInt(values[option], dimensionsAndBounds[option]) ||
            (option <= WidthB && dimensionsAndBounds[option] <= 0)) {
            cerr << "error: invalid integer for " << names[option] << '\n';
            return 1;
        }
    }

    const int min = dimensionsAndBounds[Min];
    const int max = dimensionsAndBounds[Max];
    if (min >= max) {
        cerr << "error: --min must be smaller than --max\n";
        return 1;
    }
    if (strcmp(values[PathA], values[PathB]) == 0) {
        cerr << "error: --pathA and --pathB must differ\n";
        return 1;
    }

    try {
        const bool saved = GenerateAndWrite(dimensionsAndBounds[HeightA], dimensionsAndBounds[WidthA],
                                            min, max, values[PathA], "Generating matrix A", "Writing matrix A") &&
                           GenerateAndWrite(dimensionsAndBounds[HeightB], dimensionsAndBounds[WidthB],
                                            min, max, values[PathB], "Generating matrix B", "Writing matrix B");
        return saved ? 0 : 1;
    } catch (const exception& error) {
        cerr << "error: matrix generation failed: " << error.what() << '\n';
        return 1;
    }
}
