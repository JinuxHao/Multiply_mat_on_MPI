#include "myMat.h"
#include "Generate/progress_bar.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>

using namespace std;

Mat Transpose(const Mat* matrix)
{
    Mat transposed;
    if (matrix == nullptr || matrix->data == nullptr ||
        !AllocMat(&transposed, matrix->W, matrix->H)) {
        return {};
    }

    for (int i = 0; i < transposed.H; ++i)
        for (int j = 0; j < transposed.W; ++j)
            transposed.data[i][j] = matrix->data[j][i];
    return transposed;
}

void InitMat(Mat* matrix)
{
    *matrix = Mat{};
}

bool AllocMat(Mat* matrix, int height, int width)
{
    if (matrix == nullptr || height <= 0 || width <= 0 ||
        static_cast<size_t>(height) > numeric_limits<size_t>::max() / sizeof(float*) ||
        static_cast<size_t>(width) > numeric_limits<size_t>::max() / sizeof(float)) {
        cerr << "error: invalid matrix allocation " << height << 'x' << width << '\n';
        return false;
    }

    ReleaseMat(matrix);
    matrix->data = static_cast<float**>(calloc(static_cast<size_t>(height), sizeof(float*)));
    if (matrix->data == nullptr) {
        cerr << "error: failed to allocate matrix " << height << 'x' << width << '\n';
        return false;
    }

    matrix->H = height;
    matrix->W = width;
    for (int i = 0; i < height; ++i) {
        matrix->data[i] = static_cast<float*>(malloc(static_cast<size_t>(width) * sizeof(float)));
        if (matrix->data[i] == nullptr) {
            cerr << "error: failed to allocate matrix " << height << 'x' << width << '\n';
            ReleaseMat(matrix);
            return false;
        }
    }
    return true;
}

bool WriteMat(const char* path, Mat matrix, const char* label)
{
    if (path == nullptr || matrix.data == nullptr || matrix.H <= 0 || matrix.W <= 0) {
        cerr << "error: invalid matrix or path for writing\n";
        return false;
    }

    ofstream output(path, ios::binary);
    if (!output) {
        cerr << "error: failed to open " << path << " for writing\n";
        return false;
    }

    output.write(reinterpret_cast<const char*>(&matrix.H), sizeof(matrix.H));
    output.write(reinterpret_cast<const char*>(&matrix.W), sizeof(matrix.W));

    const size_t rowBytes = static_cast<size_t>(matrix.W) * sizeof(float);
    ProgressBar progress(static_cast<size_t>(matrix.H) * matrix.W, label);
    for (int row = 0; row < matrix.H && output; ++row) {
        if (output.write(reinterpret_cast<const char*>(matrix.data[row]), rowBytes)) {
            progress.tick(matrix.W);
            progress.display();
        }
    }

    output.close();
    if (!output) {
        cerr << "error: failed to write matrix to " << path << '\n';
        return false;
    }
    progress.done();
    return true;
}

bool Write_to_txt(const char* path, Mat result)
{
    if (path == nullptr || result.data == nullptr || result.H <= 0 || result.W <= 0) {
        cerr << "error: invalid result matrix or output path\n";
        return false;
    }

    ofstream output(path);
    if (!output) {
        cerr << "error: failed to open " << path << " for writing\n";
        return false;
    }

    output << result.H << '\n' << result.W << '\n';
    for (int i = 0; i < result.H && output; ++i) {
        for (int j = 0; j < result.W; ++j)
            output << result.data[i][j] << ' ';
        output << '\n';
    }

    output.close();
    if (!output) {
        cerr << "error: failed to write result to " << path << '\n';
        return false;
    }
    cout << "Result matrix written to " << path << '\n';
    return true;
}

bool ReadMat(const char* path, Mat* matrix)
{
    if (matrix != nullptr)
        ReleaseMat(matrix);
    if (path == nullptr || matrix == nullptr) {
        cerr << "error: invalid matrix or path for reading\n";
        return false;
    }

    ifstream input(path, ios::binary | ios::ate);
    if (!input) {
        cerr << "error: failed to open " << path << " for reading\n";
        return false;
    }

    const streamoff fileSize = input.tellg();
    input.seekg(0);
    int height = 0, width = 0;
    if (!input.read(reinterpret_cast<char*>(&height), sizeof(height)) ||
        !input.read(reinterpret_cast<char*>(&width), sizeof(width)) ||
        height <= 0 || width <= 0) {
        cerr << "error: invalid binary matrix header in " << path << '\n';
        return false;
    }

    const uint64_t rowBytes = static_cast<uint64_t>(width) * sizeof(float);
    const uint64_t expectedSize = sizeof(height) + sizeof(width) + rowBytes * height;
    if (fileSize < 0 || static_cast<uint64_t>(fileSize) != expectedSize ||
        rowBytes > static_cast<uint64_t>(numeric_limits<streamsize>::max())) {
        cerr << "error: invalid binary matrix size in " << path << '\n';
        return false;
    }
    if (!AllocMat(matrix, height, width))
        return false;

    ProgressBar progress(static_cast<size_t>(height) * width, "Reading");
    for (int row = 0; row < height; ++row) {
        if (!input.read(reinterpret_cast<char*>(matrix->data[row]), static_cast<streamsize>(rowBytes))) {
            cerr << "error: failed to read matrix data from " << path << '\n';
            ReleaseMat(matrix);
            return false;
        }
        progress.tick(width);
        progress.display();
    }

    progress.done();
    cout << "Loaded matrix " << height << 'x' << width << " from " << path << '\n';
    return true;
}

void ReleaseMat(Mat* matrix)
{
    if (matrix == nullptr)
        return;
    if (matrix->data != nullptr) {
        for (int row = 0; row < matrix->H; ++row)
            free(matrix->data[row]);
        free(matrix->data);
    }
    InitMat(matrix);
}

Mat GenMat(int height, int width, float min, float max, const char* label)
{
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<float> dis(min, max);

    Mat matrix;
    if (!AllocMat(&matrix, height, width))
        return matrix;

    ProgressBar progress(static_cast<size_t>(height) * width, label);
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j)
            matrix.data[i][j] = dis(gen);
        progress.tick(width);
        progress.display();
    }

    progress.done();
    return matrix;
}
