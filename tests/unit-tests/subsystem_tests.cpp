#include "test_harness.h"

#include "core/Subsystem.h"

#include <string>
#include <vector>

namespace
{

class ProbeSubsystem : public Subsystem
{
public:
    ProbeSubsystem(std::vector<std::string> *log, std::string name)
        : log_(log)
        , name_(std::move(name))
    {
    }

    void Init() override
    {
        log_->push_back("init:" + name_);
    }

    void Shutdown() override
    {
        log_->push_back("shutdown:" + name_);
    }

    void ResetForMatch() override
    {
        log_->push_back("reset:" + name_);
    }

    const std::string &Name() const
    {
        return name_;
    }

private:
    std::vector<std::string> *log_;
    std::string name_;
};

class ValueSubsystem : public Subsystem
{
public:
    explicit ValueSubsystem(int value)
        : value(value)
    {
    }

    int value = 0;
};

} // namespace

void RunSubsystemTests()
{
    // --- InitAll runs forward, Shutdown runs reverse then clears ---
    {
        std::vector<std::string> log;
        Subsystems systems;
        systems.Add<ProbeSubsystem>(&log, "a");
        systems.AddKeyed<ProbeSubsystem>("second", &log, "b");
        CC_CHECK(systems.Size() == 2);

        systems.InitAll();
        CC_CHECK(log.size() == 2);
        CC_CHECK(log[0] == "init:a");
        CC_CHECK(log[1] == "init:b");

        systems.Shutdown();
        CC_CHECK(log.size() == 4);
        CC_CHECK(log[2] == "shutdown:b");
        CC_CHECK(log[3] == "shutdown:a");
        CC_CHECK(systems.Size() == 0);
    }

    // --- Get / Has / TryGet for present and absent types ---
    {
        Subsystems systems;
        CC_CHECK(!systems.Has<ValueSubsystem>());
        CC_CHECK(systems.TryGet<ValueSubsystem>() == nullptr);

        ValueSubsystem &added = systems.Add<ValueSubsystem>(42);
        CC_CHECK(systems.Has<ValueSubsystem>());
        CC_CHECK(systems.TryGet<ValueSubsystem>() == &added);
        CC_CHECK(systems.Get<ValueSubsystem>().value == 42);
        CC_CHECK(&systems.Get<ValueSubsystem>() == &added);
    }

    // --- keyed lookup keeps repeated types distinct ---
    {
        std::vector<std::string> log;
        Subsystems systems;
        ProbeSubsystem &ai = systems.AddKeyed<ProbeSubsystem>("ai", &log, "ai");
        ProbeSubsystem &ally = systems.AddKeyed<ProbeSubsystem>("ally", &log, "ally");
        ProbeSubsystem &enemy2 = systems.AddKeyed<ProbeSubsystem>("enemy2", &log, "enemy2");
        CC_CHECK(&systems.GetKeyed<ProbeSubsystem>("ai") == &ai);
        CC_CHECK(&systems.GetKeyed<ProbeSubsystem>("ally") == &ally);
        CC_CHECK(&systems.GetKeyed<ProbeSubsystem>("enemy2") == &enemy2);
        CC_CHECK(systems.GetKeyed<ProbeSubsystem>("ai").Name() == "ai");
        CC_CHECK(systems.GetKeyed<ProbeSubsystem>("ally").Name() == "ally");
        CC_CHECK(systems.GetKeyed<ProbeSubsystem>("enemy2").Name() == "enemy2");
        CC_CHECK(!systems.Has<ProbeSubsystem>());
    }

    // --- ResetForMatch fans out to every subsystem in order ---
    {
        std::vector<std::string> log;
        Subsystems systems;
        systems.Add<ProbeSubsystem>(&log, "a");
        systems.AddKeyed<ProbeSubsystem>("second", &log, "b");
        systems.ResetForMatch();
        CC_CHECK(log.size() == 2);
        CC_CHECK(log[0] == "reset:a");
        CC_CHECK(log[1] == "reset:b");
    }
}
