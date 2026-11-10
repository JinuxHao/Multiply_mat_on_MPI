/* 矩阵相关头文件 */
#pragma once
#include <iostream>
#include <cstring>

typedef struct Mat {
    int H =0;
    int W =0;
    float** data = NULL;
} Mat; // 定义矩阵结构体，包含高度、宽度和数据指针

Mat Transpose(Mat* matrix); // 转置矩阵

// 初始化矩阵结构体，将高度、宽度和数据指针设置为默认值
void InitMat(Mat* matrix);

/* 按“第一行 H，第二行 W，随后 H 行、每行 W 个数值”的格式保存矩阵。*/
int WriteMat(const char* path, Mat matrix, const char* label);

/*乘积计算后将结果保存到txt文件*/
bool Write_to_txt(const char* path, Mat result);

/* 读取 WriteMat 写出的矩阵文件。成功返回 1；
 * 文件、格式、尺寸或内存分配失败时返回 0，并保持空矩阵状态。
 * 调用前 matrix 必须已初始化；已有数据会先被释放。 */
int ReadMat(const char* path, Mat* matrix);

/* 按 height x width 分配空矩阵。成功返回 1；
 * 尺寸非法时返回 0 且不改变 matrix；分配失败时释放已有数据并恢复为空矩阵。
 * 调用前 matrix 必须已初始化；已有数据会先被释放。 */
int AllocMat(Mat* matrix, int height, int width);

/* 逐行释放数据和行指针数组，然后恢复空矩阵状态 */
void ReleaseMat(Mat* matrix);

/* 生成一个指定尺寸和数值范围的随机矩阵。成功返回 1；失败返回 0 */
Mat GenMat(int height, int width, float min, float max,
    const char* label = NULL);