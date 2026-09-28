#pragma once

struct PerfSample
{
    double simMs = 0.0;
    double uiUpdateMs = 0.0;
    double uiRenderMs = 0.0;
    int uiTriangles = 0;
};

class CheatOverlay
{
public:
    CheatOverlay() = default;
    CheatOverlay(const CheatOverlay &) = delete;
    CheatOverlay &operator=(const CheatOverlay &) = delete;

#if defined(CC_DEBUG)
    void Init();
    void Shutdown();
    void Frame(const PerfSample &perf);
    bool CapturingInput() const;
#else
    void Init() {}
    void Shutdown() {}
    void Frame(const PerfSample &) {}
    bool CapturingInput() const { return false; }
#endif

private:
    bool open_ = false;
};
