#include "../myMat.h"
#include "args_parser.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <omp.h>

using namespace std;

static void Usage()
{
    cout << "Usage: multiply_omp --pathA <matrix_a> --pathB <matrix_b> [--pathC <output_file>]\n"
        << "       multiply_omp -h|--help\n"
        << "Reads binary A and B and writes a text result (default: C.txt).\n"
        << "Set OMP_NUM_THREADS to control the OpenMP thread count.\n"
        << "Example: ./multiply_omp --pathA A.bin --pathB B.bin --pathC C.txt\n";
}

static void MultiplyMat(const Mat* left, const Mat* rightT, Mat* result)
{
    // 右矩阵已转置。每一行可独立计算。
    #pragma omp parallel for
    for (int i = 0; i < left->H; ++i) {
        if (i == 0)
            cout << "Using " << omp_get_num_threads() << " threads for multiplication." << endl;
        for (int j = 0; j < rightT->H; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < left->W; ++k)
                sum += left->data[i][k] * rightT->data[j][k];
            result->data[i][j] = sum;
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        Usage();
        return 0;
    }

    const char* pathA = nullptr;
    const char* pathB = nullptr;
    const char* pathC = nullptr;
    if (!Parse_arg(argc, argv, &pathA, &pathB, &pathC)) {
        Usage();
        return 1;
    }

    Mat left, right, result;
	int status = 1; // 默认为失败，除非成功完成所有操作。
    if (!ReadMat(pathA, &left) || !ReadMat(pathB, &right))
        goto cleanup;

    if (left.W != right.H) {
        cerr << "error: incompatible matrix dimensions " << left.H << 'x' << left.W
             << " and " << right.H << 'x' << right.W << '\n';
        goto cleanup;
    }

    if (!AllocMat(&result, left.H, right.W))
        goto cleanup;

    {
        cout << "Multiplying matrix A (" << left.H << 'x' << left.W << ") by matrix B ("
             << right.H << 'x' << right.W << ") ...\n";
        const auto start = chrono::high_resolution_clock::now();
        Mat rightT = Transpose(&right);
        if (rightT.data == nullptr)
            goto cleanup;

        MultiplyMat(&left, &rightT, &result);
        const auto end = chrono::high_resolution_clock::now();
        ReleaseMat(&rightT);
        const auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
        cout << "Matrix multiplication completed in " << elapsed.count() / 1000000.0 << " seconds\n";

        if (Write_to_txt(pathC, result)) 
            status = 0;
        
    }

cleanup:
    ReleaseMat(&left);
    ReleaseMat(&right);
    ReleaseMat(&result);
    return status;
}
