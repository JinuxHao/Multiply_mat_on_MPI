/* 线程安全的终端进度条（header-only）。
 *
 * API 与显示风格参考 progresscpp：
 *   https://github.com/prakhar1989/progress-cpp (MIT License)
 * 用法是 `++bar; bar.display(); ... bar.done();`。
 *
 * 与上游 progresscpp 的主要差异（为了本项目的老 Linux / MPI 部署）：
 *   - 线程安全：ticks 用 std::atomic 累加，绘制用 std::mutex 串行化，
 *     多个工作线程可以同时 tick()/display()，不会写乱同一条进度条；
 *   - 输出到 stderr，并且只在 stderr 连接终端时绘制；重定向、管道或
 *     CI 日志中自动静默，不会弄脏输出；
 *   - display() 内部按进度节流，每个阶段最多重绘约 100 次，调用 1000 万次
 *     也不会明显拖慢生成；
 *   - 环境变量 CALMAT_PROGRESS=1/always/force 强制开启，0/off 强制关闭；
 *     TERM=dumb 时自动关闭；
 *   - 仅使用 ASCII 字符，老终端和不同代码页下都能正常显示。
 */
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

class ProgressBar {
public:
    /* total 为总刻度数；label 只在绘制时使用，需比进度条活得久。
     * width、complete、incomplete 可自定义条形宽度和字符。 */
    ProgressBar(std::size_t total, const char* label = "",
                unsigned width = kDefaultWidth,
                char complete = '#', char incomplete = '-')
        : total_(total),
          label_(label != NULL ? label : ""),
          width_(width == 0 ? (unsigned)kDefaultWidth
                            : (width > (unsigned)kMaxWidth
                               ? (unsigned)kMaxWidth : width)),
          complete_(complete),
          incomplete_(incomplete),
          step_(total / kRedrawPerBar + 1),
          next_display_(step_),
          ticks_(0),
          finished_(false),
          last_drawn_((std::size_t)-1),
          enabled_(EnabledOn()),
          started_(Clock::now())
    {
        if (enabled_) {
            std::lock_guard<std::mutex> lock(mutex_);
            DrawLocked(0);
        }
    }

    /* 析构时若还没有 done()/cancel()，保留实际进度并换行。
     * 注意：调用方必须先结束所有工作线程，再销毁进度条。 */
    ~ProgressBar()
    {
        if (!finished_.load(std::memory_order_relaxed))
            cancel();
    }

    /* 记录 n 个刻度（默认 1）。可在多个线程中同时调用。 */
    void tick(std::size_t n = 1)
    {
        ticks_.fetch_add(n, std::memory_order_relaxed);
    }

    /* 与 progresscpp 的 ++bar 用法一致。 */
    std::size_t operator++()
    {
        tick();
        return ticks_.load(std::memory_order_relaxed);
    }

    /* 绘制当前进度；内部节流，调用开销很小，可在循环里每轮调用。
     * 多个线程同时调用也安全（互斥绘制）。 */
    void display()
    {
        if (!enabled_)
            return;
        std::size_t done = ticks_.load(std::memory_order_relaxed);
        if (done < next_display_.load(std::memory_order_relaxed) && done < total_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        done = ticks_.load(std::memory_order_relaxed);
        if (done < next_display_.load(std::memory_order_relaxed) && done < total_)
            return;
        DrawLocked(done);
        std::size_t next = done + step_;
        next_display_.store(next, std::memory_order_relaxed);
    }

    /* 标记完成：显示 100% 并换行。可重复调用，只会生效一次。 */
    void done()
    {
        if (finished_.exchange(true, std::memory_order_relaxed))
            return;
        if (!enabled_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        ticks_.store(total_, std::memory_order_relaxed);
        if (last_drawn_ != total_)
            DrawLocked(total_);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }

    /* 中途失败：保留实际进度并换行，不假装已经完成。 */
    void cancel()
    {
        if (finished_.exchange(true, std::memory_order_relaxed))
            return;
        if (!enabled_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t done = ticks_.load(std::memory_order_relaxed);
        if (done > total_)
            done = total_;
        if (last_drawn_ != done)
            DrawLocked(done);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }

    bool enabled() const
    {
        return enabled_;
    }

private:
    typedef std::chrono::steady_clock Clock;

    enum { kDefaultWidth = 40, kMaxWidth = 200, kRedrawPerBar = 100 };

    ProgressBar(const ProgressBar&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;

    /* 终端判断：显式环境变量优先，其次 isatty(2)（stderr 的 fd）。 */
    static bool EnabledOn()
    {
        const char* env = std::getenv("CALMAT_PROGRESS");
        if (env != NULL && env[0] != '\0') {
            if (std::strcmp(env, "0") == 0 || std::strcmp(env, "off") == 0 ||
                std::strcmp(env, "no") == 0 || std::strcmp(env, "false") == 0)
                return false;
            if (std::strcmp(env, "1") == 0 || std::strcmp(env, "on") == 0 ||
                std::strcmp(env, "always") == 0 || std::strcmp(env, "force") == 0 ||
                std::strcmp(env, "yes") == 0 || std::strcmp(env, "true") == 0)
                return true;
        }

        const char* term = std::getenv("TERM");
        if (term != NULL && std::strcmp(term, "dumb") == 0)
            return false;

#if defined(_WIN32)
        return _isatty(2) != 0;
#else
        return isatty(2) != 0;
#endif
    }

    /* 调用方必须持有 mutex_。 */
    void DrawLocked(std::size_t done)
    {
        char bar[kMaxWidth + 1];
        char line[512];
        double fraction;
        int percent, filled;
        unsigned i;
        double elapsed;

        if (done > total_)
            done = total_;
        fraction = total_ == 0 ? 1.0 : (double)done / (double)total_;
        /* 用截断而不是四舍五入，避免未完成时提前显示 100%。 */
        percent = (int)(fraction * 100.0);
        filled = (int)(fraction * (double)width_);
        if (filled > (int)width_)
            filled = (int)width_;
        for (i = 0; i < width_; ++i)
            bar[i] = i < (unsigned)filled ? complete_ : incomplete_;
        bar[width_] = '\0';

        elapsed = std::chrono::duration<double>(Clock::now() - started_).count();
        std::snprintf(line, sizeof(line),
                      "%s [%s] %3d%% %10llu/%llu %7.1fs",
                      label_, bar, percent,
                      (unsigned long long)done,
                      (unsigned long long)total_, elapsed);
        /* 末尾空格用于覆盖上一行残留的更宽数字。 */
        std::fprintf(stderr, "\r%s   ", line);
        std::fflush(stderr);
        last_drawn_ = done;
    }

    const std::size_t total_;
    const char* const label_;
    const unsigned width_;
    const char complete_;
    const char incomplete_;
    const std::size_t step_;

    std::atomic<std::size_t> next_display_;
    std::atomic<std::size_t> ticks_;
    std::atomic<bool> finished_;
    std::size_t last_drawn_; /* 只在 mutex_ 保护下读写 */
    const bool enabled_;
    const Clock::time_point started_;

    std::mutex mutex_;
};
