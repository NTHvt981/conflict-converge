
#include "app/ui/RmlUiMenus.h"

#include <cstdio>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "app/input/Hotkeys.h"
#include "app/match/Menu.h"

void RmlUiMenus::BeginRemap(MenuState returnTo)
{
    remapArming_ = -1;
    remapConflictAction_.clear();
    remapReturn_ = returnTo;
    Announce(EventType::MenuAction);
    menu_.state = MenuState::HotkeyRemap;
}

bool RmlUiMenus::CancelRemapCapture()
{
    if (!ready_ || menu_.state != MenuState::HotkeyRemap)
    {
        return false;
    }
    if (remapArming_ >= 0)
    {
        remapArming_ = -1;
    }
    else
    {
        remapConflictAction_.clear();
        menu_.state = remapReturn_;
    }
    return true;
}

void RmlUiMenus::RefreshRemap()
{
    const std::string signature = RemapSignature();
    if (signature != remapCache_)
    {
        remapCache_ = signature;
        PopulateRemapRows();
    }
    if (remapArming_ >= 0)
    {
        SetTextIn(remapDoc_, "remap-status", "Press a key for the armed action (Esc cancels)");
    }
    else if (!remapConflictAction_.empty())
    {
        const std::optional<std::string> owner = hotkeys_.ActionForKey(remapConflictKey_);
        char text[128];
        snprintf(text, sizeof(text), "'%s' already fires '%s' - click the row again to steal it.",
                 GetKeyName(remapConflictKey_), owner.has_value() ? owner->c_str() : "?");
        SetTextIn(remapDoc_, "remap-status", text);
    }
    else
    {
        SetTextIn(remapDoc_, "remap-status",
                  "Click a key to rebind it. Digits/Alt (Tier 2) are fixed.");
    }
}

std::string RmlUiMenus::RemapSignature() const
{
    std::string signature;
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        signature += kHotkeyDefs[i].action;
        signature += '=';
        signature += std::to_string(hotkeys_.KeyFor(kHotkeyDefs[i].action));
        signature += ';';
    }
    signature += "armed=" + std::to_string(remapArming_);
    signature += ";conflict=" + remapConflictAction_;
    return signature;
}

void RmlUiMenus::PopulateRemapRows()
{
    const int defCount = NumHotkeyDefs();
    const int perCol = (defCount + 1) / 2;
    Rml::Element *cols[2] = {
        remapDoc_->GetElementById("hkcol-a"),
        remapDoc_->GetElementById("hkcol-b"),
    };
    for (int col = 0; col < 2; ++col)
    {
        while (Rml::Element *child = cols[col]->GetFirstChild())
        {
            cols[col]->RemoveChild(child);
        }
    }
    for (int i = 0; i < defCount; ++i)
    {
        const HotkeyDef &def = kHotkeyDefs[i];
        Rml::ElementPtr row(remapDoc_->CreateElement("div"));
        char rowId[32];
        snprintf(rowId, sizeof(rowId), "hkrow-%d", i);
        row->SetAttribute("id", Rml::String(rowId));
        row->SetAttribute("class", Rml::String("hkrow"));
        if (remapArming_ == i)
        {
            row->SetClass("armed", true);
        }
        Rml::ElementPtr label(remapDoc_->CreateElement("span"));
        label->SetAttribute("class", Rml::String("hklabel"));
        label->SetInnerRML(def.label);
        Rml::ElementPtr key(remapDoc_->CreateElement("button"));
        char keyId[32];
        snprintf(keyId, sizeof(keyId), "hkkey-%d", i);
        key->SetAttribute("id", Rml::String(keyId));
        key->SetAttribute("class", Rml::String("hkkey"));
        const int effective = hotkeys_.KeyFor(def.action);
        std::string keyText = effective <= 0 ? "-" : GetKeyName(effective);
        if (def.chord)
        {
            keyText = "Shift+" + keyText;
        }
        if (remapArming_ == i)
        {
            keyText = "press a key...";
        }
        key->SetInnerRML(keyText.c_str());
        key->AddEventListener("click", this);
        Rml::Element *rowRaw = row.get();
        rowRaw->AppendChild(std::move(label));
        rowRaw->AppendChild(std::move(key));
        cols[i / perCol]->AppendChild(std::move(row));
    }
}

void RmlUiMenus::PollRemapCapture()
{
    if (remapArming_ < 0)
    {
        return;
    }
    // Mirror the raygui capture: Esc/modifiers never bind; the queue drains
    // here after input_.Update already ran (shortcuts processed first).
    int pressed = 0;
    int candidate = 0;
    while ((pressed = GetKeyPressed()) != 0)
    {
        if (pressed != KEY_ESCAPE && pressed != KEY_LEFT_SHIFT &&
            pressed != KEY_RIGHT_SHIFT && pressed != KEY_LEFT_CONTROL &&
            pressed != KEY_RIGHT_CONTROL && pressed != KEY_LEFT_ALT &&
            pressed != KEY_RIGHT_ALT)
        {
            candidate = pressed;
            break;
        }
    }
    if (candidate == 0)
    {
        return;
    }
    const std::string action = kHotkeyDefs[remapArming_].action;
    const std::optional<std::string> owner = hotkeys_.ActionForKey(candidate);
    if (owner.has_value() && *owner != action)
    {
        remapConflictAction_ = action;
        remapConflictKey_ = candidate;
    }
    else
    {
        hotkeys_.Rebind(action, candidate);
        callbacks_.hotkeysRebound();
        menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
        SaveSettings(menu_.settings, kSettingsPath);
        remapArming_ = -1;
    }
}
