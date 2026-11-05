# 矩阵生成与五次性能测试

四个脚本均放在项目根目录，与 `Generate`、`Multiply`、`matrices`、`results` 同级。无需 Python，也无需安装 Excel。

```text
Mat_Multiplacation/
  generate_matrices.ps1       Windows 生成
  generate_matrices.sh        Linux 生成
  benchmark_matrices.ps1      Windows 测试
  benchmark_matrices.sh       Linux 测试
  BATCH_USAGE.md
  Makefile                   Linux 编译入口
  generate                   Linux 生成程序（make 后产生）
  multiply                   Linux 计算程序（make 后产生）
  Generate/
    Generate.cpp
    x64/Debug/Generate.exe    当前 Windows 编译输出
  Multiply/
    Multiply.cpp
    x64/Debug/Multiply.exe
  matrices/
  results/
```

所有可配置路径必须是相对路径，均相对于脚本所在的项目根目录解释，而非启动命令时的工作目录。路径包含空格时加引号。脚本可从其他目录启动，复制整个项目后不需改盘符或用户名。

## Windows：PowerShell 5.1 或更新版本

先在 Visual Studio 中编译 Generate、Multiply。脚本优先查找各自的 `x64/Release`，再查找 `x64/Debug`，并在启动时显示实际选择的程序。若两种版本都存在，建议显式指定本次要测试的版本，避免测试旧程序。

在项目根目录运行：

```powershell
# 预览九对的随机尺寸、总字节数及生成命令，不创建文件
.\generate_matrices.ps1 -Seed 42 -DryRun

# 正式生成九对，写入 matrices
.\generate_matrices.ps1 -Seed 42

# 检查输入与五次调用计划，不执行乘法
.\benchmark_matrices.ps1 -DryRun

# 所有矩阵对各计算五次
.\benchmark_matrices.ps1 -Label openmp
```

如果系统提示禁止执行脚本，可使用仅作用于本次 PowerShell 进程的启动方式：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\generate_matrices.ps1 -Seed 42
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\benchmark_matrices.ps1 -Label openmp
```

显式指定现有编译输出：

```powershell
.\generate_matrices.ps1 -GeneratorPath .\Generate\x64\Debug\Generate.exe -Seed 42
.\benchmark_matrices.ps1 -MultiplierPath .\Multiply\x64\Debug\Multiply.exe -Label debug
```

其他参数示例：

```powershell
# 只生成百级三对，并使用独立输入目录
.\generate_matrices.ps1 -Levels hundreds -Seed 42 -MatricesDir matrices_small -Min -10 -Max 10
.\benchmark_matrices.ps1 -MatricesDir matrices_small -ResultsDir results -Label small
```

`-Levels` 接受 `all`、`hundreds`、`thousands`、`ten-thousands`，默认 `all`。不指定种子时用当前秒级时间生成种子。

## Linux：按当前结构迁移，用 Makefile 编译

可以只迁移必需的 `.h`、`.cpp` 和 Makefile，并保持以下相对结构；若要批量生成和测试，再带上两个 `.sh`。Windows 的 `.slnx`、`.vcxproj`、`.vs`、`x64`、`.exe` 等不需要用于 Linux 编译，已有的矩阵和结果也可以不上传，在服务器重新生成。

```text
Mat_Multiplacation/
  Makefile
  myMat.h
  myMat.cpp
  Generate/
    Generate.cpp
    progress_bar.h
  Multiply/
    Multiply.cpp
  generate_matrices.sh
  benchmark_matrices.sh
```

服务器需要 GNU make、支持 C++11 的 G++、OpenMP 运行库和 Bash 4+。当前 MPI 头文件及初始化已注释，默认编译不需要 MPI。下面均在项目根目录执行：

```bash
# 先查看将要执行的编译命令
make -n

# 编译两个程序，也可以用 make -j4 并行构建
make

# 单独构建某个程序
make generate
make multiply
```

编译产物为根目录的 `./generate`、`./multiply`，中间 `.o` 和自动生成的头文件依赖 `.d` 放在 `.build` 中。Makefile 所有输入、输出路径均为相对路径。修改源码或头文件后再次 `make` 即可增量编译。

当前 myMat.h 已包含 `<cstring>`，Generate.cpp 和 Multiply.cpp 都包含该共享头文件，`strcmp` 的声明已具备。本次只添加 Makefile、更新 Linux 脚本及说明，不修改 C++ 源码，也不通过编译选项强制包含头文件。

默认参数为 `-O2 -std=c++11 -Wall -Wextra -fopenmp -pthread`。由于共享的 myMat.cpp 含 `omp_get_num_threads()`，生成程序也需要链接 OpenMP。当前代码若只想先做单线程基准，可在运行前 `export OMP_NUM_THREADS=1`。

```bash
# 预览
bash ./generate_matrices.sh --seed 42 --dry-run

# 生成九对
bash ./generate_matrices.sh --seed 42

# 检查配对，再开始五次测试
bash ./benchmark_matrices.sh --dry-run
bash ./benchmark_matrices.sh --label openmp
```

默认可执行路径为根目录的 `./generate`、`./multiply`。注意 Linux 区分大小写，它们与源码目录 `Generate/`、`Multiply/` 并存。如编译到其他位置，用相对路径覆盖：

```bash
bash ./generate_matrices.sh --generator ./build/Generate --seed 42
bash ./benchmark_matrices.sh --multiplier ./build/Multiply --label serial

# 单独生成和测试百级样例
bash ./generate_matrices.sh --levels hundreds --seed 42 \
  --matrices-dir matrices_small --min -10 --max 10
bash ./benchmark_matrices.sh --matrices-dir matrices_small \
  --results-dir results --label small
```

使用 `bash 文件名` 不需要执行权限；也可先 `chmod +x generate_matrices.sh benchmark_matrices.sh` 再直接调用。`.sh` 文件使用 LF 换行，传输时保留；若编辑器改为 CRLF，可执行 `sed -i 's/\r$//' generate_matrices.sh benchmark_matrices.sh`。

Makefile 同样保留 LF 换行，编译命令行开头为 Tab。`make clean` 只清理两个可执行程序和已知的中间构建文件，不删除 matrices 或 results。

以后若恢复 MPI 代码，先加载服务器上的 MPI 开发环境，再使用 MPI 的 C++ 包装编译器：

```bash
make clean
make CXX=mpicxx
```

切换编译器或编译选项前先 `make clean`，避免复用旧的对象文件。Makefile 只负责编译；MPI 的进程数、启动器及通信逻辑仍由你配置。批量脚本默认直接启动程序，不会自动添加 mpiexec。

## 随机尺寸与数据格式

| 级别 | 默认的 m、k、n 随机区间 | 文件编号 |
| --- | --- | --- |
| 百级 | 100～999 | 100、101、102 |
| 千级 | 1000～9999 | 1000、1001、1002 |
| 万级 | 10000～19999 | 10000、10001、10002 |

每级各三对：随机阶数方阵、结果为高矩形、结果为宽矩形。矩形样例的 m、k、n 两两不同，始终满足 `A(m×k) × B(k×n) = C(m×n)`。尺寸不要求偶数或分块大小的倍数。

文件保持 `A_100.bin`、`B_100.bin` 等命名。**编号不等于行列数**，实际尺寸记录在 `matrices/cases.csv`。两个生成脚本采用相同的维度随机算法，同种子、同级别选择及同区间会得到相同尺寸。C++ 自行生成随机元素，脚本种子不保证复现矩阵元素。

如需缩小区间，Windows 版修改 `$lower`、`$upper`，Linux 版修改 `LOWER`、`UPPER`。例如上界改为 `499、1999、11999`；每个区间至少包含三个不同正整数。生成九对前应先用预览查看容量：默认种子 42 约需 4.89 GiB 输入空间。结果 TXT 体积通常更大。

二进制格式为两个 int32 行列数，随后按行存储 float32 元素，大小为 `8 + 4×行数×列数` 字节。当前格式面向 Windows 与 Linux x86_64 的小端平台。生成脚本验证文件头与大小后发布矩阵对；若目标编号或 `cases.csv` 已存在，会拒绝覆盖。可以选另一个输入目录，中断后已经完成的矩阵对会保留。

## 性能指标和结果文件

测试脚本不依赖 `cases.csv`，扫描所有数字编号的 `A_<id>.bin`、`B_<id>.bin`，按编号数值排序。先检查配对、文件长度和 `A.width == B.height`；检查未通过则不启动任何乘法。

每对连续启动五次 Multiply，每次都会读取输入、计算、写出 TXT。**记录的是程序报告的内部耗时**：

```text
Matrix multiplication completed in 0.123seconds
```

允许 `1.23e-4seconds` 及数字和 `seconds` 间的空格。当前 C++ 计时包围整个 `MultiplyMat`，其中包含转置和函数内分配，不包括读取输入、写出结果。脚本的启动、文件验证、日志保存等时间不计入 CSV，不记录外部总耗时。

每批创建独立目录，例如：

```text
results/<时间戳>-<标签>-<唯一标识>/
  C_100.txt                    每对只保留最后一次成功结果
  C_101.txt
  ...
  runs.csv                     每次耗时与状态
  summary.csv                  五次耗时与算术平均值
  logs/case_100_run1.log
  logs/case_100_run2.log
  ...
```

CSV 使用 UTF-8，包含表头，可用 Excel 打开；若 Excel 按单列显示，用“数据 → 从文本/CSV”，选择逗号分隔。时间单位统一为秒。CSV 中的结果和日志路径仍相对于项目根目录，而非 CSV 所在目录。

- `runs.csv`：label、id、m、k、n、run、seconds、status、exit_code、log。
- `summary.csv`：label、id、m、k、n、run1_seconds～run5_seconds、average_seconds、successful_runs、status、result。

每次都检查程序退出码、TXT 的尺寸、完整行数、每行列数和数值格式，并且要求恰好一条有效计时输出。失败记录的 seconds 为空，继续其余次数和矩阵对；五次全部成功才填写平均值。若第五次失败，保留此前最后一次成功的 TXT。没有额外预热、自动重试或异常值剔除。

当前 C++ 的成功返回码为 `1`，脚本据此判断；脚本自身成功返回 `0`、存在失败返回 `1`。如果以后将 C++ 改为成功返回 `0`，调用时指定：

```powershell
.\generate_matrices.ps1 -SuccessExitCode 0
.\benchmark_matrices.ps1 -SuccessExitCode 0
```

```bash
bash ./generate_matrices.sh --success-exit-code 0
bash ./benchmark_matrices.sh --success-exit-code 0
```

脚本不会设置 OpenMP 线程数，也不替你切换算法；继承调用环境的 `OMP_NUM_THREADS` 等设置。测试时关闭终端进度条，其他日志保留。建议性能比较使用同一份已生成的输入及 Release/优化编译，标签可区分不同实现。

当前脚本直接启动指定的 Multiply。未来 MPI 版本的启动器、进程数、通信及仅主进程输出等由你配置；每次调用应只输出一条汇总计时记录，避免将多个 rank 的时间误当成单次测量。脚本不自动生成 `mpiexec` 启动配置。

## 本次验证范围

在 Windows PowerShell 5.1 和 Git Bash 中，使用项目现有 Generate/Multiply 可执行文件验证了百级矩阵生成、小型方阵与两种矩形各五次计算，并独立核对小矩阵的数值结果。额外验证了跨平台尺寸一致、含空格路径、不同工作目录、已有文件保护、缺失配对、损坏矩阵、失败退出码、缺失或多条计时输出、科学计数法、失败平均值留空及最后一次成功结果保留。

Linux 脚本已通过 Bash 语法和 Git Bash 实际调用验证；尚未在原生 Linux 环境实测。没有执行完整万级矩阵的五次乘法测试。验证产物保留在 Jinux_code 的本任务目录，不放入项目数据目录。

后续添加的 Makefile 已通过 GNU make 命令展开及本机 GCC 构建检查；最新源码也通过了 GCC 语法检查。该检查不是 Linux 原生运行验证。
