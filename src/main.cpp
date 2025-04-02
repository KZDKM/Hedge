#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>

inline HANDLE pHandle;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

typedef SDispatchResult(*tSpawn)(std::string);

tSpawn pSpawn;

enum eHotEdgeSide {
    TOP,
    BOTTOM,
    LEFT,
    RIGHT
};

struct SHotEdgeConfig {
    // monitor the hot edge is on
    std::string monitor;
    // which side of the monitor the edge is on
    eHotEdgeSide side;
    // size of the zone perpendicular to the edge that activates the edge when entered
    int activateZoneSize;
    // size of the zone that deactivates the edge when left
    int deactivateZoneSize;
    // command to execute when activated
    std::string activateCommand;
    // command to execute when deactivated
    std::string deactivateCommand;
    // should the edge be always active if no windows are in the deactvation zone
    bool dodgeWindow;
};

// global list of hot edges defined in the config, should be cleared on config reload
std::vector<std::tuple<SHotEdgeConfig, bool>> g_HotEdges;

// global state register for zone trigger, unused
bool g_isCursorInZone = false;

bool g_isMouseMoved = false;

void onMouseMove(void* thisptr, SCallbackInfo& info, std::any args) {
    if (std::any_cast<Vector2D>(args).distance({0, 0}) > 0.5)
        g_isMouseMoved = true;
}

// update state once how many period
int tickPeriod = 20;
uint64_t tickCounter = 0;

void tick() {
    if ((tickCounter % tickPeriod == 0)) {
        for (auto& edgeConfig : g_HotEdges) {
            auto edge = std::get<0>(edgeConfig);
            auto state = std::get<1>(edgeConfig);
            auto monitor = g_pCompositor.get()->getMonitorFromName(edge.monitor);
            if (monitor && g_pCompositor.get()->getMonitorFromCursor() == monitor) {
                const auto pos = g_pInputManager.get()->getMouseCoordsInternal();
                CBox activationZone;
                CBox deactivationZone;
                Vector2D monitorPos = monitor.get()->vecPosition;
                Vector2D monitorSize = monitor.get()->vecSize;
                switch (edge.side) {
                case eHotEdgeSide::TOP:
                    activationZone = CBox(monitorPos, Vector2D{monitorSize.x, (float)edge.activateZoneSize});
                    deactivationZone = CBox(monitorPos, Vector2D{monitorSize.x, (float)edge.deactivateZoneSize});
                    break;
                case eHotEdgeSide::BOTTOM:
                    activationZone = CBox(Vector2D{monitorPos.x, monitorPos.y + monitorSize.y - edge.activateZoneSize}, Vector2D{monitorSize.x, (float)edge.activateZoneSize});
                    deactivationZone = CBox(Vector2D{monitorPos.x, monitorPos.y + monitorSize.y - edge.deactivateZoneSize}, Vector2D{monitorSize.x, (float)edge.deactivateZoneSize});
                    break;
                case eHotEdgeSide::LEFT:
                    activationZone = CBox(monitorPos, Vector2D{(float)edge.activateZoneSize, monitorSize.y});
                    deactivationZone = CBox(monitorPos, Vector2D{(float)edge.deactivateZoneSize, monitorSize.y});
                    break;
                case eHotEdgeSide::RIGHT:
                    activationZone = CBox(Vector2D(monitorPos.x + monitorSize.x - edge.activateZoneSize, monitorPos.y), Vector2D{monitorSize.x, (float)edge.activateZoneSize});
                    deactivationZone = CBox(Vector2D(monitorPos.x + monitorSize.x - edge.deactivateZoneSize, monitorPos.y), Vector2D{monitorSize.x, (float)edge.deactivateZoneSize});
                    break;
                }
                bool isCursorInActivationZone = activationZone.containsPoint(pos);
                bool isCursorInDeactivationZone = deactivationZone.containsPoint(pos);

                // Check windows intersection
                bool isIntersectingWindows = false;
                bool isWorkspaceFullscreen = false;
                if (edge.dodgeWindow) {
                    auto workspace = monitor.get()->activeWorkspace;
                    if (workspace) {
                        isWorkspaceFullscreen = workspace.get()->m_efFullscreenMode == eFullscreenMode::FSMODE_FULLSCREEN;
                        for (PHLWINDOW window : g_pCompositor.get()->m_vWindows) {
                            if (window && window.get()->workspaceID() == workspace.get()->m_iID) {
                                if (!CBox(window.get()->m_vRealPosition.get()->value(), window.get()->m_vRealSize.get()->value()).intersection(deactivationZone).empty()) {
                                    isIntersectingWindows = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!isWorkspaceFullscreen) {
                    if (!state && (isCursorInActivationZone || (edge.dodgeWindow && !isIntersectingWindows))) {
                        pSpawn(edge.activateCommand);
                        state = true;
                    }
                    else if (g_isCursorInZone && !isCursorInDeactivationZone && (!edge.dodgeWindow || isIntersectingWindows)) {
                        pSpawn(edge.deactivateCommand);
                        state = false;
                    }
                    std::get<1>(edgeConfig) = state;
                }

            }
        }
        g_isMouseMoved = false;
    }
    tickCounter++;
}

void reloadConfig() {
    g_isCursorInZone = false;
    g_HotEdges.clear();
}

// the "command" arg should be ignored
// value is formatted as follows
// ordered as is, separated by comma:
// monitor (e.g. DP-1)
// side (top, botom, left, right)
// activateZoneSize (int)
// deactivateZoneSize (int)
// activateCommand (string)
// deactivateCommand (string)
// dodgeWindow (bool)
Hyprlang::CParseResult handleHotEdgeKeyword(const char* command, const char* value) {
    auto result = Hyprlang::CParseResult();
    auto stream = std::stringstream(std::string(value));
    SHotEdgeConfig config;
    std::string arg;
    int i;
    for (i = 0; std::getline(stream, arg, ','); i++) {
        switch (i) {
        case 0:
            config.monitor = arg;
            break;
        case 1:
            if (arg == "top") {
                config.side = eHotEdgeSide::TOP;
            }
            else if (arg == "bottom") {
                config.side = eHotEdgeSide::BOTTOM;
            }
            else if (arg == "left") {
                config.side = eHotEdgeSide::LEFT;
            }
            else if (arg == "right") {
                config.side = eHotEdgeSide::RIGHT;
            }
            else {
                result.setError("hotedge: config did not specify a valid side");
                return result;
            }
            break;
        case 2:
            config.activateZoneSize = std::stoi(arg);
            break;
        case 3:
            config.deactivateZoneSize = std::stoi(arg);
            break;
        case 4:
            config.activateCommand = arg;
            break;
        case 5:
            config.deactivateCommand = arg;
            break;
        case 6:
            config.dodgeWindow = std::stoi(arg);
            break;
        default:
            break;
        }
    }

    // malformed
    if (i + 1 != 8) result.setError("hotedge: config has too few arguments");
    else {
        g_HotEdges.push_back(std::make_tuple(config, false));
    }

    return result;
}

void* findFunctionBySymbol(HANDLE inHandle, const std::string func, const std::string sym) {
    // should return all functions
    auto funcSearch = HyprlandAPI::findFunctionsByName(inHandle, func);
    for (auto f : funcSearch) {
        if (f.demangled.contains(sym))
            return f.address;
    }
    return nullptr;
}

Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> configReloadHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> mouseMoveHook;

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;
    Hyprlang::SHandlerOptions handlerOptions;
    handlerOptions.allowFlags = false;
    HyprlandAPI::addConfigKeyword(pHandle, "hotedge", handleHotEdgeKeyword, handlerOptions);
    pSpawn = (tSpawn)findFunctionBySymbol(pHandle, "spawn", "CKeybindManager::spawn(");
    configReloadHook = HyprlandAPI::registerCallbackDynamic(pHandle, "preConfigReload", [&] (void* thisptr, SCallbackInfo& info, std::any data) { reloadConfig(); });
    configReloadHook = HyprlandAPI::registerCallbackDynamic(pHandle, "tick", [&] (void* thisptr, SCallbackInfo& info, std::any data) { tick(); });
    mouseMoveHook = HyprlandAPI::registerCallbackDynamic(pHandle, "mouseMove", onMouseMove);
    return {"Hedge", "Hyprland hot edge", "KZdkm", "0.1"};
}
APICALL EXPORT void PLUGIN_EXIT() {
}