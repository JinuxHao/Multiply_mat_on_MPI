/* 矩阵相关头文件 */
#pragma once

struct Mat {
    int H = 0;
    int W = 0;
    float** data = nullptr;
};

// 转置矩阵，分配失败时返回空矩阵。
Mat Transpose(const Mat* matrix);

// 初始化尚未分配数据的矩阵。
void InitMat(Mat* matrix);

// 写入二进制矩阵，先写 H、W，再按行写入 float 元素。
bool WriteMat(const char* path, Mat matrix, const char* label);

// 写入文本矩阵，前两行分别为 H 和 W，随后每行写 W 个数值。
bool Write_to_txt(const char* path, Mat result);

// 读取二进制矩阵。目标须已初始化，原数据会释放，失败时目标为空。
bool ReadMat(const char* path, Mat* matrix);

// 分配矩阵。尺寸非法时保留目标，分配失败时目标为空。
bool AllocMat(Mat* matrix, int height, int width);

// 释放矩阵并恢复为空矩阵。
void ReleaseMat(Mat* matrix);

// 生成指定尺寸和范围的随机矩阵，分配失败时返回空矩阵。
Mat GenMat(int height, int width, float min, float max, const char* label = nullptr);
