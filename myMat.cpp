#include "myMat.h"
#include <fstream>

#include <random> // 用于生成随机数
#include "./Generate/progress_bar.h"

#include <omp.h> // 乘积函数可能需要并行化

using namespace std;

Mat Transpose(Mat* matrix)
{
    Mat newMatrix;
    InitMat(&newMatrix);// 初始化新矩阵
    if (matrix == NULL || !AllocMat(&newMatrix, matrix->W, matrix->H)) {
		cout << "error: failed to allocate memory for transposed matrix " << endl;
        return Mat();
    }
    for (int i = 0; i < newMatrix.H; i++)
        for (int j = 0; j < newMatrix.W; j++)
            newMatrix.data[i][j] = matrix->data[j][i];

    return newMatrix;
}

void InitMat(Mat* matrix)
{
    matrix->H = 0;
    matrix->W = 0;
    matrix->data = NULL;
}


int AllocMat(Mat* matrix, int height, int width)
{
    if (matrix == NULL || height <= 0 || width <= 0) {
		cout << "error: invalid matrix dimensions " << height << "x" << width << endl;
		return 0;
    }

    cout << "Allocating matrix " << height << "x" << width << " ...\n";

    InitMat(matrix);
    matrix->H = height;
    matrix->W = width;

	// 先分配height个行指针，再分配每行的width个float
	matrix->data = (float**)malloc((size_t)height * sizeof(float*));//calloc 分配内存并初始化为0
    if (matrix->data == NULL)
        return 0;

    for (int i = 0; i < height; i++) {
		matrix->data[i] = (float*)malloc((size_t)width * sizeof(float)); //malloc 分配内存
        if (matrix->data[i] == NULL) {
            ReleaseMat(matrix);
            return 0;
        }
    }
    return 1;
}

int WriteMat(const char* path, Mat matrix, const char* label)
{

    if (path == NULL || matrix.data == NULL ||
        matrix.H <= 0 || matrix.W <= 0) {
		cerr << "error: invalid matrix or path for writing." << endl;
        return 0;
    }

	ofstream myout(path, ios::out | ios::binary); // 写入二进制文件
	if (myout.fail()) {
		cerr << "error: failed to open file " << path << " for writing." << endl;
		return 0;
	}

	// 写入矩阵的高度和宽度
	myout.write(reinterpret_cast<const char*>(&matrix.H), sizeof(int));
	myout.write(reinterpret_cast<const char*>(&matrix.W), sizeof(int));

    /* 写文件通常比生成更慢，所以保存阶段同样显示进度。 */
	const size_t total = (size_t)matrix.H * (size_t)matrix.W; // 获得总元素数来显示进度条
    ProgressBar progress(total, label); 
    for (int row = 0; row < matrix.H; ++row) {
        for (int column = 0; column < matrix.W; ++column) {
			myout.write(reinterpret_cast<const char*>(&matrix.data[row][column]), sizeof(float));
            progress.tick();
            progress.display();
        }
    }

    /* 只有整份文件写完才显示 100%，失败则保留实际进度。 */
    progress.done();

	return 1;
    
}

int ReadMat(const char* path, Mat* matrix)
{
	cout << "Reading matrix from " << path << " ...\n";

    if (path == NULL || matrix == NULL) {
        cerr << "error: invalid matrix or path for reading." << endl;
        return 0;
    }

	ifstream myin(path, ios::in|ios::binary);
	if (!myin.is_open()) {
		cerr << "error: failed to open file " << path << endl;
		return 0;
	}

    int height = 0, width = 0;

	//读取矩阵的高度和宽度，两个int类型
	myin.read(reinterpret_cast<char*>(&height), sizeof(int));
	myin.read(reinterpret_cast<char*>(&width), sizeof(int));

	if (!AllocMat(matrix, height, width)) {
		cout << "error: failed to allocate memory for matrix " << height << "x" << width << endl;
		myin.close();
		return 0;
	}

	const size_t total = (size_t)height * (size_t)width; // 进度条按元素计数
	ProgressBar progress(total, "Reading"); 
	for (int row = 0; row < height; ++row) {
		for (int column = 0; column < width; ++column) {
			// 读取每个元素，float类型
			myin.read(reinterpret_cast<char*>(&matrix->data[row][column]), sizeof(float));
			progress.tick();
			progress.display();
		}
	}

	progress.done();
	myin.close();
    
    cout << "Loaded matrix " << height << "x" << width << " from " << path << endl;
    return 1;
}

/* 逐行释放数据和行指针数组，然后恢复空矩阵状态。 */
void ReleaseMat(Mat* matrix)
{
    int row;

    if (matrix == NULL)
        return;
    if (matrix->data != NULL) {
        for (row = 0; row < matrix->H; ++row)
            free(matrix->data[row]);
        free(matrix->data);
    }
    matrix->H = 0;
    matrix->W = 0;
    matrix->data = NULL;
}

int MultiplyMat(const Mat* left, const Mat* right, Mat* result)
{
    /*先对右乘矩阵进行转置，方便按行访问 */
    Mat rightT;
    InitMat(&rightT);
    rightT = Transpose((Mat*)right);
    if (rightT.data == NULL)
        return 0;

    //开始计算矩阵乘法
    //#pragma omp parallel for
    for (int i = 0; i < left->H; i++) {
        if (i == 0) // 在第一行输出线程数，便于调试和性能分析
			cout << "Using " << omp_get_num_threads() << " threads for multiplication." << endl;
        for (int j = 0; j < rightT.H; j++) {
            float sum = 0.0;
            for (int k = 0; k < left->W; k++) {
                sum += left->data[i][k] * rightT.data[j][k];
            }
            result->data[i][j] = sum;
        }
    }

    ReleaseMat(&rightT); // 释放转置矩阵的内存
    return 1;
}

Mat GenMat(int height, int width, float min, float max, const char* label)
{
    // 创建一个随机数生成器
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<float> dis(min, max);

    // 初始化矩阵，尺寸非法或分配失败时返回空矩阵
    Mat matrix;
    InitMat(&matrix);
    if (!AllocMat(&matrix, height, width))
        return matrix;

    /* 进度条按元素计数，只在 stderr 是终端时显示，避免污染脚本输出。
     * tick()/display() 都是线程安全的，将来改成多线程生成也不用改这里。 */
    const std::size_t total = (std::size_t)height * (std::size_t)width;
    ProgressBar progress(total, label);

    // 填充随机数
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            matrix.data[i][j] = dis(gen);
            progress.tick();
            progress.display();
        }
    }
    progress.done();

    return matrix;
}
