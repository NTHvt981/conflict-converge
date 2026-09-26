
#include "core/GameRenderer.h"

GameRenderer::GameRenderer(Art &art, GameCamera &camera, TileMap &map, Registry &registry,
                           FogOfWar &fog, ResourceNodes &nodes, Minimap &minimap, Pings &pings,
                           DamageNumbers &damageNumbers, PlayingInput &playingInput, MenuFlow &menu,
                           Simulation &sim, ResourceSystem &resources, ProductionQueue &queue,
                            HotkeyMap &hotkeys, InputManager &input, AICommander &ai,
                            EventDispatcher &events, const int &replayCursor,
                            const AIDifficulty &worldDifficulty, const float &menuStateTime,
                            const float &shakeTrauma, RmlUiHost &rmlUi, RmlUiHud &rmlUiHud,
                            RmlUiMenus &rmlUiMenus, std::function<void()> quitToMenu)
    : art_(art)
    , camera_(camera)
    , map_(map)
    , registry_(registry)
    , fog_(fog)
    , nodes_(nodes)
    , minimap_(minimap)
    , pings_(pings)
    , damageNumbers_(damageNumbers)
    , playingInput_(playingInput)
    , menu_(menu)
    , sim_(sim)
    , resources_(resources)
    , queue_(queue)
    , hotkeys_(hotkeys)
    , input_(input)
    , ai_(ai)
    , events_(events)
    , replayCursor_(replayCursor)
    , worldDifficulty_(worldDifficulty)
    , menuStateTime_(menuStateTime)
    , shakeTrauma_(shakeTrauma)
    , rmlUi_(rmlUi)
    , rmlUiHud_(rmlUiHud)
    , rmlUiMenus_(rmlUiMenus)
    , quitToMenu_(std::move(quitToMenu))
{
}

void GameRenderer::DrawWorld()
{
    UpdateCursor();
    Camera2D shaken = camera_.view;
    if (shakeTrauma_ > 0.0f)
    {
        const float amount = ShakeMagnitude(shakeTrauma_);
        shaken.offset.x += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
        shaken.offset.y += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
    }
    BeginMode2D(shaken);
    DrawTerrainTiles();
    DrawBuildings();
    DrawPlacementGhost();
    DrawNodes();
    DrawUnits();
    DrawProjectiles();
    art_.ParticlesPool().Draw();
    damageNumbers_.Draw();
    DrawFogVeil();
    EndMode2D();
}

ConfirmChoice GameRenderer::DrawHudAndOverlays(int screenWidth, int screenHeight, float uiScale)
{
    DrawDragPreviews();
    DrawMinimapOverlays(screenWidth, screenHeight);
    sim_.RefreshFactory();
    ConfirmChoice hudChoice = ConfirmChoice::None;
    if (rmlUiHud_.IsReady())
    {
        hudChoice = rmlUiHud_.Draw(screenWidth, screenHeight, uiScale);
    }
    DrawHoverTooltip();
    DrawStateOverlays(screenWidth, screenHeight);
    return hudChoice;
}
