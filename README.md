# 矩阵乘法并行化与性能测试

使用 C++ 实现矩阵生成、文件读写和矩阵乘法，围绕**串行、OpenMP 并行、MPI 并行**三种计算方式开展性能比较。项目提供 Windows、Linux 两套批量脚本，支持随机形状的矩阵对生成、每对五次重复计算、结果保存与耗时汇总。

计算方式由 C++ 源码实现和编译配置决定；批量脚本负责外部调用，不自动切换算法。测试标签用于区分实验批次，例如 `serial`、`openmp12`、`mpi4`。

## 项目目录结构

```text
Mat_Multiplacation/
├── README.md
├── LICENSE
├── Makefile                    # Linux 构建入口
├── myMat.h                     # Mat 结构与矩阵操作声明
├── myMat.cpp                   # 分配、释放、生成、读写、转置和乘法
├── Generate/
│   ├── Generate.cpp            # 矩阵对生成程序入口
│   ├── progress_bar.h          # 终端进度条
│   ├── Generate.slnx
│   └── Generate.vcxproj        # Visual Studio 工程
├── Multiply/
│   ├── Multiply.cpp            # 矩阵乘法、计时和 TXT 输出入口
│   ├── Multiply.slnx
│   └── Multiply.vcxproj
├── generate_matrices.ps1       # Windows 批量生成
├── generate_matrices.sh        # Linux 批量生成
├── benchmark_matrices.ps1      # Windows 五次重复测试
├── benchmark_matrices.sh       # Linux 五次重复测试
├── matrices/                   # 输入矩阵与生成清单
├── results/                    # 结果、日志及性能汇总
├── generate                    # Linux 编译生成的可执行文件
└── multiply                    # Linux 编译生成的可执行文件
```

Linux 的 `generate`、`multiply` 与源码目录 `Generate/`、`Multiply/` 区分大小写。构建中间文件放在 `.build/`，无需将 Windows 的 `.exe`、`.vs`、`x64` 或 Visual Studio 工程配置迁移到 Linux。

## 项目说明

矩阵对满足：

```text
A：m × k
B：k × n
C = A × B：m × n
```

`Mat` 使用 `float**` 按行分配单精度矩阵。共享代码提供随机生成、二进制读写、转置与释放等操作；乘法实现将右矩阵转置，使内层点积能够按行访问数据，基本计算量为 `O(mkn)`。

性能实验比较三种方式：

| 计算方式 | 比较重点 |
| --- | --- |
| 串行 | 作为性能基线 |
| OpenMP | 共享内存多线程计算，比较不同线程数的耗时 |
| MPI | 多进程计算，比较不同进程数及通信开销的影响 |

使用同一组矩阵和相同计时范围进行比较。每对矩阵重复运行五次，记录全部耗时及算术平均值，不额外预热、不自动剔除异常值。

## Quick Start

以下命令均在项目根目录执行。先用百级矩阵确认编译、计算与结果输出正常，再扩大规模。示例使用 `matrices_demo`，不会覆盖已有的 `matrices` 数据。

### Linux

需要 GNU make、支持 C++11 的 G++、OpenMP 运行库及 Bash 4+；MPI 实验另需相应的 MPI 开发和运行环境。

```bash
# 编译，生成根目录的 generate 和 multiply
make -j4

# 预览百级三对矩阵的尺寸及生成命令
bash ./generate_matrices.sh --levels hundreds --seed 42 \
  --matrices-dir matrices_demo --dry-run

# 正式生成
bash ./generate_matrices.sh --levels hundreds --seed 42 \
  --matrices-dir matrices_demo

# 检查配对和五次运行计划，不执行乘法
bash ./benchmark_matrices.sh --matrices-dir matrices_demo --dry-run

# 在选择并编译好串行实现后执行基准测试
bash ./benchmark_matrices.sh --matrices-dir matrices_demo --label serial
```

### Windows

使用 Visual Studio 的 C++ 开发环境，分别打开 `Generate/Generate.slnx`、`Multiply/Multiply.slnx` 编译两个程序。工程当前指定工具集 `v145` 和 C++20；性能实验建议统一使用 `x64 Release`。

Multiply 工程配置了 OpenMP，并保留 `MSMPI_INC`、`MSMPI_LIB64`、`msmpi.lib` 等 Microsoft MPI 构建依赖。按现有工程构建时，需要让相关 SDK、库及运行环境可用；若本机没有对应工具集或 MPI 环境，应先调整构建环境。

PowerShell 脚本要求 5.1 或更新版本。下面显式选择 Release 可执行文件，避免误用其他构建版本：

```powershell
.\generate_matrices.ps1 -GeneratorPath .\Generate\x64\Release\Generate.exe `
  -Levels hundreds -Seed 42 -MatricesDir matrices_demo -DryRun

.\generate_matrices.ps1 -GeneratorPath .\Generate\x64\Release\Generate.exe `
  -Levels hundreds -Seed 42 -MatricesDir matrices_demo

.\benchmark_matrices.ps1 -MultiplierPath .\Multiply\x64\Release\Multiply.exe `
  -MatricesDir matrices_demo -Label serial
```

如果系统策略禁止直接执行脚本，可通过一次性进程参数启动，不修改系统执行策略：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\generate_matrices.ps1 -Levels hundreds -Seed 42 -MatricesDir matrices_demo
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\benchmark_matrices.ps1 -MatricesDir matrices_demo -Label serial
```

未指定可执行路径时，Windows 脚本依次查找各自的 `x64/Release`、`x64/Debug`，最后查找源码子目录下的 `.exe`，启动时会显示选择的路径。

## 编译与单次运行

### Makefile

所有源码及输出路径均为相对路径，默认构建参数为 `-O2 -std=c++11 -Wall -Wextra -fopenmp -pthread`。生成程序也链接共享的 `myMat.cpp`，因此同样需要 OpenMP 支持。

```bash
make                 # 构建两个程序
make generate        # 只构建生成程序
make multiply        # 只构建计算程序
make -n              # 查看构建命令，不执行编译
make clean           # 清理可执行程序与已知中间文件，保留矩阵和结果
```

Makefile 使用 `.d` 文件跟踪头文件依赖；修改 `.cpp`、`.h` 后再次 `make` 即可增量编译。切换编译器、优化选项或实验模式时，先清理再重建。MPI 实现可以使用服务器提供的 C++ 包装编译器，例如：

```bash
make clean
make CXX=mpicxx
```

这只改变构建工具，具体的 MPI 分工、通信和结果汇总仍需由相应的源码实现。

### 单独生成矩阵对

生成程序接收八组参数；`--min`、`--max` 是整数形式的随机值上下界，实际元素通过浮点均匀分布生成。

```bash
mkdir -p matrices results

./generate \
  --pathA matrices/A_manual.bin --pathB matrices/B_manual.bin \
  --heightA 100 --widthA 200 \
  --heightB 200 --widthB 150 \
  --min -1 --max 1
```

### 单独计算矩阵乘法

```bash
./multiply \
  --pathA matrices/A_manual.bin \
  --pathB matrices/B_manual.bin \
  --pathC results/C_manual.txt
```

`--pathA`、`--pathB` 必须指定；省略 `--pathC` 时写入当前工作目录的 `C.txt`。两个程序不会自动创建输出文件的父目录，单独运行时应先创建目录，并确保 `A.width == B.height`。

Windows 单次运行使用对应 `.exe`，例如 `.\Multiply\x64\Release\Multiply.exe`，参数名称相同。

**返回码约定：当前两个 C++ 程序成功返回 `1`，失败返回 `0`；批量脚本自身则成功返回 `0`，失败返回 `1`。** 因此不要直接用 `./generate ... && ./multiply ...` 串联两个程序，也不要在启用 `set -e` 的自定义 Shell 脚本中忽略这个约定。

## 批量矩阵生成

默认生成百、千、万级各三对，共九对、十八个二进制文件。每级的行列维度独立取自以下区间：

| 级别 | m、k、n 的范围（含端点） | 样例编号 |
| --- | --- | --- |
| 百级 | 100～999 | 100、101、102 |
| 千级 | 1000～9999 | 1000、1001、1002 |
| 万级 | 10000～19999 | 10000、10001、10002 |

每级依次包含随机阶数方阵、结果为高矩形（`m > n`）、结果为宽矩形（`m < n`）三类。后两类的 m、k、n 互不相同，且始终保持可相乘。

文件命名为 `A_100.bin / B_100.bin` 等。**后缀是样例编号，不代表实际阶数**；维度、形状、种子及文件名记录在输入目录的 `cases.csv` 中。相同种子、级别选择和尺寸范围可在 Windows、Linux 复现相同的尺寸安排；C++ 元素随机数独立播种，不保证复现相同元素。

| 用途 | Windows 参数 | Linux 参数 | 默认值 |
| --- | --- | --- | --- |
| 生成程序相对路径 | `-GeneratorPath` | `--generator` | 自动查找 `.exe` / `./generate` |
| 矩阵输出目录 | `-MatricesDir` | `--matrices-dir` | `matrices` |
| 维度种子 | `-Seed` | `--seed` | 当前秒级时间 |
| 规模选择 | `-Levels` | `--levels` | `all` |
| 随机值上下界 | `-Min`、`-Max` | `--min`、`--max` | `-1`、`1` |
| 预览 | `-DryRun` | `--dry-run` | 不启用 |
| C++ 成功返回码 | `-SuccessExitCode` | `--success-exit-code` | `1` |

规模可选 `all`、`hundreds`、`thousands`、`ten-thousands`。完整生成示例：

```bash
bash ./generate_matrices.sh --seed 42 --dry-run
bash ./generate_matrices.sh --seed 42
```

所有脚本路径参数相对于脚本所在的项目根目录解析，从其他工作目录启动也适用。单独运行 C++ 程序时，路径则相对于当前工作目录。

脚本会在执行前显示预期文件总字节数，并检查生成后的文件头和长度。已有同名矩阵或 `cases.csv` 时拒绝覆盖；需要另一次生成时指定新的输入目录。若要改变默认维度范围，可同步调整 PowerShell 的 `$lower`、`$upper` 和 Bash 的 `LOWER`、`UPPER`，每个区间至少包含三个不同的正整数。

## 批量性能测试

测试脚本从输入目录扫描数字编号的 `A_<id>.bin`、`B_<id>.bin`，按编号排序并配对，不依赖 `cases.csv`。未编号的 `A.bin`、`B.bin` 或 `A_manual.bin`、`B_manual.bin` 不会自动纳入批量测试。

开始计算前，脚本验证所有配对是否完整、二进制文件头和长度是否合法、内维是否匹配。通过检查后，逐对执行：

1. 连续启动五次计算程序，每次独立读取输入并写出结果。
2. 分别保留五次运行日志，提取每一次内部耗时。
3. 检查结果 TXT 的尺寸、行列数量和数值格式。
4. 保留每对最后一次成功的 TXT；仅五次全部成功时计算平均值。

```bash
bash ./benchmark_matrices.sh --dry-run
bash ./benchmark_matrices.sh --label serial
```

| 用途 | Windows 参数 | Linux 参数 | 默认值 |
| --- | --- | --- | --- |
| 计算程序相对路径 | `-MultiplierPath` | `--multiplier` | 自动查找 `.exe` / `./multiply` |
| 矩阵输入目录 | `-MatricesDir` | `--matrices-dir` | `matrices` |
| 结果父目录 | `-ResultsDir` | `--results-dir` | `results` |
| 实验标签 | `-Label` | `--label` | `default` |
| 仅检查运行计划 | `-DryRun` | `--dry-run` | 不启用 |
| C++ 成功返回码 | `-SuccessExitCode` | `--success-exit-code` | `1` |

标签允许英文字母、数字、下划线和连字符，不会改变线程数、进程数或算法实现。脚本遇到某次计算失败、结果无效、计时缺失或重复时，会记录失败并继续后续次数；失败项耗时留空，整批最终返回非零状态。五次未全部成功的矩阵对不填写平均值。

### 结果目录和统计字段

每次测试创建独立批次目录，不覆盖之前的实验：

```text
results/<时间戳>-<标签>-<唯一标识>/
├── C_100.txt
├── C_101.txt
├── ...
├── runs.csv
├── summary.csv
└── logs/
    ├── case_100_run1.log
    ├── case_100_run2.log
    └── ...
```

- `runs.csv`：`label,id,m,k,n,run,seconds,status,exit_code,log`，每次调用一行。
- `summary.csv`：`label,id,m,k,n,run1_seconds,...,run5_seconds,average_seconds,successful_runs,status,result`，每对矩阵一行。

CSV 使用 UTF-8、逗号分隔，可在 Excel 中打开；耗时单位统一为秒。CSV 中的日志和结果路径相对于项目根目录。每次记录及时写入 `runs.csv`，每对五次完成后写入一行汇总；若作业中途结束，已完成的记录保留，不会自动恢复剩余次数。

### 计时范围

当前计算程序对 `MultiplyMat` 调用进行计时，输出格式为：

```text
Matrix multiplication completed in 0.123seconds
```

脚本支持科学计数法和数值与 `seconds` 之间的空格。记录的是程序报告的内部耗时，包含该调用内的转置、分配和释放等操作，不包含输入读取、结果 TXT 写入、程序启动及脚本校验。切换 MPI 实现时，应统一计时范围，并由主进程输出一条能够代表整个并行计算阶段的计时记录。

## 三种计算方式的实验组织

每次切换计算实现后重新构建，保留相同输入矩阵、优化级别及计时范围，使用不同标签保存结果。以串行平均耗时 `T_serial` 为基线，可计算加速比 `S_p = T_serial / T_p`；同口径下，效率为 `E_p = S_p / p`，其中 p 是参与比较的线程数或进程数。

### 串行和 OpenMP

在选择相应的 C++ 实现并重新编译后运行。OpenMP 线程数由环境或程序设置控制，脚本继承调用环境，不自行设置线程数。例如，测试 12 线程：

```bash
export OMP_NUM_THREADS=12
export OMP_DYNAMIC=FALSE
bash ./benchmark_matrices.sh --label openmp12
```

Windows PowerShell 中对应设置为：

```powershell
$env:OMP_NUM_THREADS = '12'
$env:OMP_DYNAMIC = 'FALSE'
.\benchmark_matrices.ps1 -Label openmp12
```

### MPI

MPI 模式需要同时准备相应的计算实现、MPI 编译环境和多进程启动方式。批量脚本默认直接调用指定程序，不自动添加 `mpiexec` 或决定进程数。

Linux 上可以额外提供一个可执行的启动包装器，并通过 `--multiplier` 指向它。包装器需完整转发 `--pathA`、`--pathB`、`--pathC` 等参数；MPI 程序应由约定的进程写出最终结果和唯一计时记录，并让脚本期望的返回码与启动器实际返回码一致。

不同 MPI 实现的进程映射和集群调度规则可能不同，应依据所在服务器配置启动命令；单纯把标签改为 `mpi4` 或使用 `mpicxx` 编译不会自动实现分布式计算。

## 文件格式

### 输入矩阵 `.bin`

按当前 Windows/Linux 常见 x86_64 平台的表示方式：

| 内容 | 类型 | 长度 |
| --- | --- | --- |
| 行数 H | `int32` | 4 字节 |
| 列数 W | `int32` | 4 字节 |
| H×W 个逐行元素 | `float32` | 4×H×W 字节 |

每份文件总长度为 `8 + 4×H×W` 字节。C++ 写入使用本机字节序，没有额外的跨架构转换；批量脚本面向常见的小端平台。不要把 TXT 结果重命名成 `.bin` 作为输入。

### 结果矩阵 `.txt`

第一行为行数，第二行为列数，随后每行一个矩阵行，元素以空格分隔。例如：

```text
2
3
1 2 3
4 5 6
```

输出使用 C++ 流的默认浮点格式，不是无损的二进制存储；做数值正确性比较时应使用适当的浮点误差容限。

## 使用注意事项

- 单独调用程序时传入完整参数。Generate 的无参数、帮助及缺值处理仍不完整，可能导致非法访问；Multiply 对参数缺值等异常输入也尚未全面校验。
- 手动计算应先确认维度相容；批量脚本会预检，但 C++ 入口不提供同等完整的输入检查。
- Generate 存在重复分配及部分内存未释放的情况，万级样例的峰值内存需求高于单份矩阵大小。批量生成按矩阵对启动独立进程，退出后由操作系统回收内存。
- 万级矩阵的输入、TXT 输出和五次计算都可能耗费较多空间与时间，先预览容量并从小规模验证开始。服务器上按调度系统分配的资源运行，作业时限应覆盖读写、校验和整批计算。
- 比较性能时固定输入和编译版本，记录线程数、进程数及节点配置；在正式测试期间避免重新编译正在使用的程序。
- 终端进度条默认仅在 stderr 连接终端时显示。`CALMAT_PROGRESS=0` 可关闭，`CALMAT_PROGRESS=1` 可强制开启；批量测试脚本会关闭进度条并保存其他程序输出。

## 许可证

仓库包含 GNU General Public License 第 3 版许可证文本，详见 [LICENSE](LICENSE)。
