#include "../myMat.h"
#include "args_parser.h"

#include <mpi.h>

#include <algorithm>
#include <exception>
#include <iomanip>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

using namespace std;

static void Usage()
{
    cout << "Usage: mpiexec -n <processes> ./multiply_mpi --pathA <matrix_a>"
        << " --pathB <matrix_b> [--pathC <output_file>]\n"
        << "       multiply_mpi -h|--help\n"
        << "Rank 0 reads binary A and B and writes a text result (default: C.txt).\n"
        << "Example: mpiexec -n 4 ./multiply_mpi --pathA A.bin --pathB B.bin --pathC C.txt\n";
}

int main(int argc, char* argv[])
{
    // 初始化 MPI，获取当前进程编号和进程总数。
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
	int inputStatus = 0; // 0: 未就绪，1: 就绪，2: 帮助

    // 由 0 号进程解析参数、读入矩阵并验证维度。
    const char* pathC = nullptr;
    Mat left, right;

    if (rank == 0) {
        const char* pathA = nullptr;
        const char* pathB = nullptr;
        if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
            Usage();
            inputStatus = 2;
        } 
        else if (!Parse_arg(argc, argv, &pathA, &pathB, &pathC)) {
            Usage();
        } 
        else if (ReadMat(pathA, &left) && ReadMat(pathB, &right)) 
		{
            if (left.W != right.H) 
                cerr << "error: incompatible matrix dimensions " << left.H << 'x' << left.W << " and " << right.H << 'x' << right.W << '\n';
            else {
                inputStatus = 1;
            }
        }
    }

    // 3. 同步输入状态，确保所有进程一起继续或退出。
    MPI_Bcast(&inputStatus, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if (inputStatus != 1) { // 不是就绪状态，退出
        ReleaseMat(&left);
        ReleaseMat(&right);
        MPI_Finalize();
        return inputStatus == 2 ? 0 : 1; // 
    }

    // 同步各进程的计时起点，输入读取已经完成。
    MPI_Barrier(MPI_COMM_WORLD);
    const double start = MPI_Wtime();

	int dims[3] = { left.H, left.W, right.W }; //已确定左矩阵列数==右矩阵行数
    // 广播矩阵维度，让每个进程知道 A、B 和 C 的形状。
    MPI_Bcast(dims, 3, MPI_INT, 0, MPI_COMM_WORLD);
    const int rows = dims[0], inner = dims[1], cols = dims[2];
    if (rank == 0) { // 查看 MPI 进程数
        cout << "MPI processes: " << size << endl;
		cout << "Matrix A: " << rows << 'x' << inner << ", Matrix B: " << inner << 'x' << cols
			<< ", Matrix C: " << rows << 'x' << cols << endl;
    }

    // 按行分工，base 是每个进程平均分配的行数，remainder 是余数。
    const int base = rows / size, remainder = rows % size;
	const int localRows = base + (rank < remainder ? 1 : 0); // 当前进程负责的行数

	// aCounts: 每个进程接收的 A 元素个数，aOffsets: 每个进程接收的 A 起始位置
    vector<int> aCounts, aOffsets, cCounts, cOffsets;
	// packedA: 0 号进程打包的 A，localA: 当前进程接收的 A
    // Bt: B 的转置，
    // localC: 当前进程计算出的 C，gatheredC: 0 号进程收集的 C
    vector<float> packedA, localA, Bt, localC, gatheredC;

    // 分配计算缓冲区，0 号进程准备连续的发送和接收数据。
    int localFailure = 0; 

	// 各个进程分配自己的 A 和 C 元素缓冲区，若 localRows 为 0，则分配 1 个元素以避免传入空指针。
	// 规模为 localRows * inner/cols 意思是每个进程负责的 A 行数乘以 A 的列数，和 C 行数乘以 C 的列数。
	localA.resize(max(1, localRows * inner)); 
    localC.resize(max(1, localRows * cols));

    if (rank == 0) {
        // 主进程按照进程数size 计算 A 分发和 C 回收所需的元素个数及位移。
        aCounts.resize(size);
        aOffsets.resize(size);
        cCounts.resize(size);
        cOffsets.resize(size);
        for (int process = 0, first = 0; process < size; ++process) {
			//first: 当前进程负责的 A 的起始行号，count: 当前进程负责的 A 的行数
            const int count = base + (process < remainder ? 1 : 0);
            aCounts[process] = count * inner;
            aOffsets[process] = first * inner;
            cCounts[process] = count * cols;
            cOffsets[process] = first * cols;
            first += count;
        }

        // Mat 的每一行独立分配，先把 A 打包到连续缓冲区。
        packedA.resize(rows * inner);
        for (int i = 0; i < rows; ++i)
			for (int j = 0; j < inner; ++j)
				packedA[i * inner + j] = left.data[i][j];
		// 释放 A 的行指针和数据，避免重复释放。
        ReleaseMat(&left);
    }

    // 每个进程分配 B 的转置缓冲区，0 号进程填入转置数据。
    Bt.resize(inner * cols);
    if (rank == 0) {
		Mat rightT = Transpose(&right); // 先转置 B，方便后续计算
		for (int i = 0; i < rightT.H; i++)
			for (int j = 0; j < rightT.W; j++)
				Bt[i * rightT.W + j] = rightT.data[i][j];
		// 分配完毕，释放 B和它的转置的行指针和数据，避免重复释放。
		ReleaseMat(&rightT);
        ReleaseMat(&right);
    }

	if (rank == 0) {
		// 0 号进程分配 gatheredC 用于收集各进程的结果。
		gatheredC.resize(rows * cols);
	}

    // 汇总各进程的缓冲区分配状态，有失败就一起退出。
    int anyFailure = 0;
    MPI_Allreduce(&localFailure, &anyFailure, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    if (anyFailure != 0) {
        ReleaseMat(&left);
        ReleaseMat(&right);
        MPI_Finalize();
        return 1;
    }

    // 分发 A 的行块，每个进程只接收自己负责的行。
    MPI_Scatterv(packedA.data(), aCounts.data(), aOffsets.data(), MPI_FLOAT, localA.data(), localRows * inner, MPI_FLOAT, 0, MPI_COMM_WORLD);

	vector<float>().swap(packedA); // 释放 packedA，避免占用内存

    // 广播 B 的转置矩阵，供各进程计算局部乘积。
    MPI_Bcast(Bt.data(), inner * cols, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 每个进程计算自己负责的 C 行块。
    for (int i = 0; i < localRows; ++i) {
        const float* aRow = localA.data() + i * inner;
        for (int j = 0; j < cols; ++j) {
            const float* bRow = Bt.data() + j * inner;
            float sum = 0;
            for (int k = 0; k < inner; ++k)
                sum += aRow[k] * bRow[k];
            localC[i * cols + j] = sum;
        }
    }

    // 将各进程的 C 行块按原顺序汇总到 0 号进程。
    MPI_Gatherv(localC.data(), localRows * cols, MPI_FLOAT,
                gatheredC.data(), cCounts.data(), cOffsets.data(), MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 结束计时，取所有进程的最大耗时。
    const double localSeconds = MPI_Wtime() - start;
    double seconds = 0;
    MPI_Reduce(&localSeconds, &seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // 由 0 号进程写出完整结果，写出成功后报告耗时。
    int status = 0;

    if (rank == 0) {
        cout << "Matrix multiplication completed in "<< setprecision(numeric_limits<double>::max_digits10) << seconds << " seconds\n";

        vector<float*> resultRows; // 0 号进程保存 C 的行指针列表
        resultRows.resize(rows);
        for (int i = 0; i < rows; ++i)
            resultRows[i] = gatheredC.data() + i * cols;
        Mat result;
        result.H = rows;
        result.W = cols;
        result.data = resultRows.data();
        if (!Write_to_txt(pathC, result)) {
            cerr << "error: failed to save result\n";
            status = 1;
        }
        
    }

    // 同步写出状态，所有进程结束 MPI 并返回同一个退出码。
    MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Finalize();
    return status;
}
