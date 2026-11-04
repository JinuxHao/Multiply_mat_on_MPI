
#include "../myMat.h"
#include <chrono> // 计时

using namespace std;

/* 输出用法 */
static void Usage(FILE* output)
{
    fprintf(output, "Usage: ./Multiply --pathA <matrix_a> --pathB <matrix_b> [--pathC <output_file>]\n");
    fprintf(output, "       ./Multiply -h|--help\n");
    fprintf(output, "Reads A and B from the given text files; --pathC defaults to C.txt.\n");
    fprintf(output, "Generate and save A/B beforehand with Mat_Generator/Mat_Generator.cpp.\n");
    fprintf(output, "Files start with H W and then rows.\n");
    fprintf(output, "Example: ./Multiply --pathA A.bin --pathB B.bin --pathC C.txt\n");
}

// 解析命令行参数，读取输入矩阵并分配结果矩阵
bool Parse_input(int argc, char** argv, const char** pathA, const char** pathB, const char** pathC);

// 输出矩阵函数，将结果矩阵保存为txt文件
bool Write_to_txt(const char* path, Mat result);

int main(int argc, char** argv)
{
    const char* pathA = NULL; // 由 --pathA 指定
    const char* pathB = NULL; // 由 --pathB 指定
    const char* pathC = "C.txt"; // 默认输出文件路径

    FILE* output = NULL; // 用于写入结果矩阵的文件指针
    Mat left, right, result; // 定义三个矩阵结构体，分别用于存储左矩阵、右矩阵和结果矩阵
    const char* error = NULL; // 用于存储错误信息
    int status = 1;

    // 初始化矩阵
    InitMat(&left);
    InitMat(&right);
    InitMat(&result);

    // 解析命令行并读取输入矩阵；返回码决定继续执行、清理后失败还是直接退出。
    bool flag = Parse_input(argc, argv, &pathA, &pathB, &pathC);
    switch (flag) {
        case false: // 解析失败，已输出错误信息
            goto cleanup;
        case true: // 解析成功，继续执行
            break;
    }

	// 从指定路径读取矩阵A和B；若失败则输出错误信息并跳转到清理环节。
    if (!ReadMat(pathA, &left)) {
		cerr << "error: failed to read matrix A from " << pathA << endl;
		goto cleanup;
    }

    if (!ReadMat(pathB, &right)) {
		cerr << "error: failed to read matrix B from " << pathB << endl;
		goto cleanup;
    }

    // 执行矩阵乘法并计时；计时变量放在独立作用域，避免 goto cleanup 跨过初始化
    {
		cout << "Multiplying matrix A (" << left.H << "x" << left.W << ") by matrix B (" << right.H << "x" << right.W << ") ...\n"; 
        auto start = chrono::high_resolution_clock::now();
        MultiplyMat(&left, &right, &result);
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
		cout << "Matrix multiplication completed in " << duration.count()/1000000.0 << "seconds" << endl;

    }


cleanup: // 清理资源环节，确保无论成功还是失败都能释放内存和关闭文件
    /* 统一关闭输出并释放三个矩阵，避免各个失败出口重复清理。 */
    if (output != NULL) {
        if (fclose(output) != 0) {
            error = "output failed";
            status = 1;
        }
        output = NULL;
    }
    ReleaseMat(&left);
    ReleaseMat(&right);
    ReleaseMat(&result);
    if (status != 0) {
        if (error == NULL)
            error = "unknown error";
        fprintf(stderr, "error: %s\n", error);
    }
    return status;
}


bool Parse_input(int argc, char** argv, const char** pathA, const char** pathB, const char** pathC){
    // 处理命令行参数，只接受 --pathA、--pathB、[--pathC] 和 -h/--help
    // 先判断 argc，避免无参数运行时 argv[1] 为 NULL 导致崩溃。
    if (argc <= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        Usage(stdout);
        return false;
    }

    /* 按名称读取参数，拒绝未知、重复、缺值和位置参数。 */
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--pathA") == 0) {
            *pathA = argv[i + 1];
        }
        else if (strcmp(argv[i], "--pathB") == 0) {
            *pathB = argv[i + 1];
        }
        else if (strcmp(argv[i], "--pathC") == 0) {
            *pathC = argv[i + 1];
        }
        else {
            fprintf(stderr, "error: invalid argument format\n");
            Usage(stderr);
            break;
        }
    }

    if (!*pathA || !*pathB) {
        fprintf(stderr, "error: --pathA and --pathB are required\n");
        Usage(stderr);
        return false;
    }

    /* 输出文件不能覆盖输入文件，否则写入会破坏输入的文本文件。 */
    if (strcmp(*pathC, *pathA) == 0 || strcmp(*pathC, *pathB) == 0) {
        cerr << "output path must differ from input paths" << endl;
        return false;
    }

	return true;
}

bool Write_to_txt(const char* path, Mat result)
{
    return false;
}
