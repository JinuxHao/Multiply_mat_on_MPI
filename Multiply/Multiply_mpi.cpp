#include "../myMat.h"
#include <mpi.h>

#include <vector> 
#include <iostream>

using namespace std;

/* 输出用法 */
static void Usage()
{
    cout << "Usage: ./Multiply_mpi --pathA <matrix_a> --pathB <matrix_b> [--pathC <output_file>]" << endl;
    cout << "       ./Multiply_mpi -h|--help" << endl;
    cout << "Reads A and B from the given text files; --pathC defaults to C.txt." << endl;
    cout << "Generate and save A/B beforehand with Mat_Generator/Mat_Generator.cpp." << endl;
    cout << "Files start with H W and then rows." << endl;
    cout << "Example: ./Multiply_mpi --pathA A.bin --pathB B.bin --pathC C.txt" << endl;
}

// 解析命令行参数，读取输入矩阵并分配结果矩阵
bool Parse_input(int argc, char** argv, const char** pathA, const char** pathB, const char** pathC);


int main(int argc, char** argv)
{
    int rank = 0, size = 1;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Mat left, right;
    InitMat(&left); InitMat(&right);

    int dims[4] = { 0, 0, 0, 0 };
    int flag = 0;
	const char* pathC = "C.txt"; // 默认输出文件路径

    if (rank == 0) {
        // 查看使用到的进程数
		cout << "MPI size: " << size << endl;

        //只让 0 号读文件，再广播
		const char* pathA = NULL, * pathB = NULL;
        flag = Parse_input(argc, argv, &pathA, &pathB, &pathC);
		if (!flag || !ReadMat(pathA, &left) || !ReadMat(pathB, &right)) {
			// 读取失败，通知其他 rank 并退出
            MPI_Abort(MPI_COMM_WORLD, 1); 
        }
        dims[0] = left.H; 
        dims[1] = left.W; 
        dims[2] = right.H; 
        dims[3] = right.W;
    }

    // 广播flag
    MPI_Bcast(&flag, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!flag) { 
        if (rank == 0) 
            cerr << "error: read failed\n"; 
        MPI_Abort(MPI_COMM_WORLD, 1); 
    }

	// 广播矩阵维度
    MPI_Bcast(dims, 4, MPI_INT, 0, MPI_COMM_WORLD);
    const int H = dims[0], K = dims[1], W = dims[3];      // C: H x W
    if (rank != 0) { 
		// 其他 rank 分配矩阵空间
        cout << "Rank " << rank << ": ";
        AllocMat(&left, dims[0], dims[1]); 
        AllocMat(&right, dims[2], dims[3]); 
    }

    // 按行广播（Mat 是 float**）
    for (int i = 0; i < dims[0]; ++i)
        MPI_Bcast(left.data[i], dims[1], MPI_FLOAT, 0, MPI_COMM_WORLD);
    for (int i = 0; i < dims[2]; ++i)
        MPI_Bcast(right.data[i], dims[3], MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 行分块（前 rem 个 rank 各多一行）
    const int base = H / size, rem = H % size;
    const int localRows = base + (rank < rem ? 1 : 0);
    const int startRow = rank * base + min(rank, rem);

    vector<int> counts(size), displs(size);
    for (int r = 0, off = 0; r < size; ++r) {
        int rows = base + (r < rem ? 1 : 0);
        counts[r] = rows * W;  displs[r] = off;  off += rows * W;
    }

    // 每个 rank 自己转置 B（也可 0 号转置后广播 B^T）
    Mat rightT = Transpose(&right);
    if (rightT.data == NULL) 
        MPI_Abort(MPI_COMM_WORLD, 1);

    // 计算本地行块（保留 OpenMP 就得到 MPI+OpenMP 混合）
    vector<float> local(max<size_t>(1, (size_t)localRows * W), 0.0f);
//#pragma omp parallel for
    for (int li = 0; li < localRows; ++li) {
        const int i = startRow + li;
        for (int j = 0; j < W; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k)
                sum += left.data[i][k] * rightT.data[j][k];
            local[(size_t)li * W + j] = sum;
        }
    }

    //汇总到 0 号（注意 counts=0 的 rank 也要合法指针）
    vector<float> all;
    if (rank == 0) all.resize((size_t)H * W);
    MPI_Gatherv(local.data(), localRows * W, MPI_FLOAT,
        rank == 0 ? all.data() : (float*)NULL,
        counts.data(), displs.data(), MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 8) 只有 0 号写文件（多进程同时写会互相截断）
    if (rank == 0) {
        Mat result; InitMat(&result);
        if (AllocMat(&result, H, W)) {
            for (int i = 0; i < H; ++i)
                for (int j = 0; j < W; ++j)
                    result.data[i][j] = all[(size_t)i * W + j];
            Write_to_txt(pathC, result);
        }
        ReleaseMat(&result);
    }

    ReleaseMat(&left); ReleaseMat(&right); ReleaseMat(&rightT);
    MPI_Finalize();                     // 9) 所有 rank 必须且只能调用一次
    return 0;
}

bool Parse_input(int argc, char** argv, const char** pathA, const char** pathB, const char** pathC) {
    // 处理命令行参数，只接受 --pathA、--pathB、[--pathC] 和 -h/--help
    // 没有参数：显示用法，不访问 argv[1]
    if (argc == 1) {
        Usage();
        return false;
    }

    // 帮助参数：此时已确认 argv[1] 存在
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
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
