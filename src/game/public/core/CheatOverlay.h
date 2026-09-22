#pragma once

class CheatOverlay
{
public:
    CheatOverlay() = default;
    CheatOverlay(const CheatOverlay &) = delete;
    CheatOverlay &operator=(const CheatOverlay &) = delete;

#if defined(CC_DEBUG)
    void Init();
    void Shutdown();
    void Frame();
    bool CapturingInput() const;
#else
    void Init() {}
    void Shutdown() {}
    void Frame() {}
    bool CapturingInput() const { return false; }
#endif

private:
    bool open_ = false;
};
