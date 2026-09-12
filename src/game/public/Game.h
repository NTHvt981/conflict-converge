#pragma once

// M1 Goal 2: project structure skeleton.
// Lifecycle hooks (Init/Update/Shutdown) will be wired to main.cpp in M2/M7.

class Game
{
public:
    void Init();
    void Update();
    void Shutdown();
    bool IsRunning() const;
};
