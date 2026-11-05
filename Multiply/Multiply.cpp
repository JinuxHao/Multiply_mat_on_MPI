
#include "../myMat.h"
#include <chrono> // 计时
#include <fstream>

//#include <mpi.h>

using namespace std;

/* 输出用法 */
static void Usage()
{
    cout<<"Usage: ./Multiply --pathA <matrix_a> --pathB <matrix_b> [--pathC <output_file>]"<<endl;
    cout<<"       ./Multiply -h|--help"<<endl;
    cout<<"Reads A and B from the given text files; --pathC defaults to C.txt."<<endl;
    cout<<"Generate and save A/B beforehand with Mat_Generator/Mat_Generator.cpp."<<endl;
    cout<<"Files start with H W and then rows."<<endl;
    cout<<"Example: ./Multiply --pathA A.bin --pathB B.bin --pathC C.txt"<<endl;
}

// 解析命令行参数，读取输入矩阵并分配结果矩阵
bool Parse_input(int argc, char** argv, const char** pathA, const char** pathB, const char** pathC);

// 输出矩阵函数，将结果矩阵保存为txt文件
bool Write_to_txt(const char* path, Mat result);

int main(int argc, char** argv)
{
	// MPI 初始化
	//int rank, size;
	//MPI_Init(&argc, &argv);
 //   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
 //   MPI_Comm_size(MPI_COMM_WORLD, &size);

    Mat left, right, result; // 定义三个矩阵结构体，分别用于存储左矩阵、右矩阵和结果矩阵
    const char* error = NULL; // 用于存储错误信息
    int status = 0;
    //int dims[4] = { 0 }; // 存储

    // 初始化矩阵
    InitMat(&left);
    InitMat(&right);
    InitMat(&result);

    // 由主进程来进行初始化，参数读取等操作
    //if (rank == 0) {

    const char* pathA = NULL; // 由 --pathA 指定
    const char* pathB = NULL; // 由 --pathB 指定
    const char* pathC = "C.txt"; // 默认输出文件路径

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

    // 为result矩阵分配空间
    if (!AllocMat(&result, left.H, right.W)) {
        cerr << "error: failed to allocate memory for result matrix " << left.H << "x" << right.W << endl;
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

	// 将结果矩阵写入指定路径的txt文件；若失败则输出错误信息并跳转到清理环节。
	if (!Write_to_txt(pathC, result)) {
		cerr << "error: failed to write result matrix to " << pathC << endl;
		goto cleanup;
	}
	status = 1; // 成功执行到此处，设置状态码为1表示成功


cleanup: // 清理资源环节，确保无论成功还是失败都能释放内存和关闭文件
    /* 统一释放三个矩阵，避免各个失败出口重复清理。 */
    ReleaseMat(&left);
    ReleaseMat(&right);
    ReleaseMat(&result);
    if (status != 1) {
        if (error == NULL)
            error = "unknown error";
        fprintf(stderr, "error: %s\n", error);
    }
    return status;
}


bool Parse_input(int argc, char** argv, const char** pathA, const char** pathB, const char** pathC){
    // 处理命令行参数，只接受 --pathA、--pathB、[--pathC] 和 -h/--help
    // 没有参数：显示用法，不访问 argv[1]
    if (argc == 1) {
        Usage();
        return false;
    }

    // 帮助参数：此时已确认 argv[1] 存在
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 ||strcmp(argv[1], "--help") == 0)) {
        Usage();
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
            Usage();
            break;
        }
    }

    if (!*pathA || !*pathB) {
        fprintf(stderr, "error: --pathA and --pathB are required\n");
        Usage();
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
	ofstream myout(path, ios::out);
    
    if (!myout.is_open()) {
        cerr << "error: failed to open file " << path << " for writing." << endl;
        return false;
    }

    // 写入矩阵的维度
    myout << result.H << endl << result.W << endl;

    // 写入矩阵的元素
    for (int i = 0; i < result.H; i++) {
        for (int j = 0; j < result.W; j++) {
            myout << result.data[i][j] << " ";
        }
        myout << endl;
    }

    myout.close();
    return true;
}
