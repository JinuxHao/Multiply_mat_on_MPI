// Generate.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "../myMat.h"
#include <iostream>
#include <fstream>
using namespace std;

static void Usage() {
	cout << "Usage: ./Generate --pathA <path_to_matrix_A> --pathB <path_to_matrix_B> --heightA <height_of_matrix_A> " << endl;
	cout<<"--widthA <width_of_matrix_A> --heightB <height_of_matrix_B> --widthB <width_of_matrix_B> --min <min_value> --max < max_value>" << endl;
	cout << "Example: ./Generate --pathA A.bin --pathB B.bin --heightA 100 --widthA 200 --heightB 200 --widthB 100 --min -1 --max 1" << endl;
}

bool Parse_input(int argc, char* argv[], int* heightA, int* widthA, int* heightB, int* widthB, int* min, int* max, const char** pathA, const char** pathB);

int main(int argc, char* argv[])
{
	int heightA  , widthA ;
	int heightB , widthB ;
	int min , max ;
	const char* pathA = NULL, *pathB = NULL;

	// 解析命令行参数
	bool flag = Parse_input(argc, argv, &heightA, &widthA, &heightB, &widthB, &min, &max, &pathA, &pathB);
	if (!flag) {
		cerr << "Parsing input failed. Please check the command line arguments." << endl;
		Usage();
		return 0;
	}

	Mat tmp_mat; // 定义一个临时矩阵结构体，用于存储生成的矩阵数据

	// 为矩阵A分配内存
	if (!AllocMat(&tmp_mat, heightA, widthA)) {
		cerr << "Failed to allocate memory for matrix A." << endl;
		return 0;
	};

	// 生成矩阵A
	tmp_mat = GenMat(heightA, widthA, min, max, "Generating matrix A");
	if (tmp_mat.data == NULL) {
		cerr << "Failed to generate matrix A." << endl;
		return 0;
	}

	// 将矩阵A写入指定路径
	if (!WriteMat(pathA, tmp_mat, "Writing matrix A")) {
		cerr << "Failed to write matrix A to file." << endl;
		return 0;
	}

	// 重置tmp_mat，以便为矩阵B分配内存
	InitMat(&tmp_mat);

	// 为矩阵B分配内存
	if (!AllocMat(&tmp_mat, heightB, widthB)) {
		cerr << "Failed to allocate memory for matrix B." << endl;
		return 0;
	};

	// 生成矩阵B
	tmp_mat = GenMat(heightB, widthB, min, max, "Generating matrix B");
	if (tmp_mat.data == NULL) {
		cerr << "Failed to generate matrix B." << endl;
		return 0;
	}

	// 将矩阵B写入指定路径
	if (!WriteMat(pathB, tmp_mat, "Writing matrix B")) {
		cerr << "Failed to write matrix B to file." << endl;
		return 0;
	}

	// 释放矩阵内存
	ReleaseMat(&tmp_mat);
	return 1;
}



bool Parse_input(int argc, char* argv[], int* heightA, int* widthA, int* heightB, int* widthB, int* min, int* max, const char** pathA, const char** pathB){
	if (argc < 17 && (strcmp(argv[1], "-h") | strcmp(argv[1], "--help")) == 0) {
		Usage();
		return false;
	}

	for (int i = 1; i < argc; i+=2) {
		if (strcmp(argv[i], "--pathA") == 0) {
			*pathA = argv[i+1];
		}
		else if (strcmp(argv[i], "--pathB") == 0) {
			*pathB = argv[i+1];
		}
		else if (strcmp(argv[i], "--heightA") == 0) {
			*heightA = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i], "--widthA") == 0) {
			*widthA = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i], "--heightB") == 0) {
			*heightB = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i], "--widthB") == 0) {
			*widthB = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i], "--min") == 0) {
			*min = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i], "--max") == 0) {
			*max = atoi(argv[i+1]);
		}
	}
	return true;
}
