#if defined(CC_DEBUG)

#include "CheatOverlay.h"
#include "Cheats.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include <string>
#include <vector>

void CheatOverlay::Init()
{
    rlImGuiSetup(true);
}

void CheatOverlay::Shutdown()
{
    rlImGuiShutdown();
}

void CheatOverlay::Frame()
{
    if (IsKeyPressed(KEY_GRAVE) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) open_ = !open_;
    if (!open_) return;
    rlImGuiBegin();
    ImGui::Begin("Cheats", &open_);
    const std::vector<cc::cheat::CheatWidgetBase *> &widgets =
        cc::cheat::CheatRegistry::Instance().Widgets();
    std::vector<std::string> files;
    for (const cc::cheat::CheatWidgetBase *w : widgets)
    {
        bool seen = false;
        for (const std::string &f : files)
        {
            if (f == w->File())
            {
                seen = true;
                break;
            }
        }
        if (!seen)
        {
            files.emplace_back(w->File());
        }
    }
    for (const std::string &file : files)
    {
        if (!ImGui::CollapsingHeader(file.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            continue;
        }
        for (cc::cheat::CheatWidgetBase *w : widgets)
        {
            if (file != w->File())
            {
                continue;
            }
            switch (w->Kind())
            {
            case cc::cheat::CheatWidgetKind::Toggle:
            {
                auto *tw = static_cast<cc::cheat::CheatWidget<cc::cheat::TOGGLE> *>(w);
                bool v = tw->value();
                if (ImGui::Checkbox(tw->Name(), &v))
                {
                    tw->Set(v);
                }
                break;
            }
            case cc::cheat::CheatWidgetKind::Int:
            {
                auto *iw = static_cast<cc::cheat::CheatWidget<cc::cheat::INT> *>(w);
                int v = iw->value();
                ImGui::InputInt(iw->Name(), &v);
                iw->Set(v);
                break;
            }
            case cc::cheat::CheatWidgetKind::Float:
            {
                auto *fw = static_cast<cc::cheat::CheatWidget<cc::cheat::FLOAT> *>(w);
                float v = fw->value();
                ImGui::DragFloat(fw->Name(), &v);
                fw->Set(v);
                break;
            }
            case cc::cheat::CheatWidgetKind::Button:
            {
                auto *bw = static_cast<cc::cheat::CheatWidget<cc::cheat::BUTTON> *>(w);
                bool clicked = ImGui::Button(bw->Name());
                bw->Set(clicked);
                break;
            }
            }
        }
    }
    ImGui::End();
    rlImGuiEnd();
}

bool CheatOverlay::CapturingInput() const
{
    return open_;
}

#endif
