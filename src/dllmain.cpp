#include "stdafx.h"
#include "helper.hpp"

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <safetyhook.hpp>

HMODULE baseModule = GetModuleHandle(NULL);
HMODULE thisModule; // Fix DLL

// Version
std::string sFixName = "BerserkFix";
std::string sFixVer = "0.0.4";
std::string sLogFile = sFixName + ".log";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::filesystem::path sExePath;
std::string sExeName;
std::filesystem::path sThisModulePath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";
std::pair DesktopDimensions = { 0,0 };

// Ini variables
bool bCustomRes;
int iCustomResX = 1280;
int iCustomResY = 720;
bool bBorderlessMode;
bool bWindowedMode;
bool bFixAspect;
bool bFixHUD;
float fFramerateCap;
float fGameplayFOVMulti;
int iShadowResolution;

// Aspect ratio + HUD stuff
float fPi = (float)3.141592653;
float fAspectRatio;
float fNativeAspect = (float)16 / 9;
float fAspectMultiplier;
float fHUDWidth;
float fHUDHeight;
float fHUDWidthOffset;
float fHUDHeightOffset;

// Variables
int iCurrentResX;
int iCurrentResY;
float fCurrentFrametime = 0.0166666f;
LPCSTR sWindowClassName = "BERSERK_WIN_EU_NA";

void CalculateAspectRatio(bool bLog)
{
    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2;
    }

    if (bLog) {
        // Log details about current resolution
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {}x{}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }   
}

// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size) {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file() {
        if (std::filesystem::exists(_filename)) {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

void Logging()
{
    // Get this module path
    WCHAR thisModulePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(thisModule, thisModulePath, MAX_PATH);
    sThisModulePath = thisModulePath;
    sThisModulePath = sThisModulePath.remove_filename();

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // spdlog initialisation
    {
        try {
            // Create 10MB truncated logger
            logger = logger = std::make_shared<spdlog::logger>(sLogFile, std::make_shared<size_limited_sink<std::mutex>>(sThisModulePath.string() + sLogFile, 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
            spdlog::info("----------");
            spdlog::info("Log file: {}", sThisModulePath.string() + sLogFile);
            spdlog::info("----------");

            // Log module details
            spdlog::info("Module Name: {0:s}", sExeName.c_str());
            spdlog::info("Module Path: {0:s}", sExePath.string());
            spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
            spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(baseModule, 1);
        }
    }
}

void Configuration()
{
    // Initialise config
    std::ifstream iniFile(sThisModulePath.string() + sConfigFile);
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVer.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sThisModulePath.string().c_str() << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Config file: {}", sThisModulePath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();
    spdlog::info("----------");

    inipp::get_value(ini.sections["Custom Resolution"], "Enabled", bCustomRes);
    inipp::get_value(ini.sections["Custom Resolution"], "Width", iCustomResX);
    inipp::get_value(ini.sections["Custom Resolution"], "Height", iCustomResY);
    inipp::get_value(ini.sections["Custom Resolution"], "Windowed", bWindowedMode);
    inipp::get_value(ini.sections["Custom Resolution"], "Borderless", bBorderlessMode);
    spdlog::info("Config Parse: bCustomRes: {}", bCustomRes);
    spdlog::info("Config Parse: iCustomResX: {}", iCustomResX);
    spdlog::info("Config Parse: iCustomResY: {}", iCustomResY);
    spdlog::info("Config Parse: bWindowedMode: {}", bWindowedMode);
    spdlog::info("Config Parse: bBorderlessMode: {}", bBorderlessMode);

    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);
    spdlog::info("Config Parse: bFixAspect: {}", bFixAspect);

    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bFixHUD);
    spdlog::info("Config Parse: bFixHUD: {}", bFixHUD);

    inipp::get_value(ini.sections["Gameplay FOV"], "Multiplier", fGameplayFOVMulti);
    if ((float)fGameplayFOVMulti < 0.10f || (float)fGameplayFOVMulti > 3.00f) {
        fGameplayFOVMulti = std::clamp((float)fGameplayFOVMulti, 0.10f, 3.00f);
        spdlog::warn("Config Parse: fGameplayFOVMulti value invalid, clamped to {}", fGameplayFOVMulti);
    }
    spdlog::info("Config Parse: fGameplayFOVMulti: {}", fGameplayFOVMulti);

    inipp::get_value(ini.sections["Framerate Cap"], "Framerate", fFramerateCap);
    if ((float)fFramerateCap < 10.00f || (float)fFramerateCap > 500.00f) {
        fFramerateCap = std::clamp((float)fFramerateCap, 10.00f, 500.00f);
        spdlog::warn("Config Parse: fFramerateCap value invalid, clamped to {}", fFramerateCap);
    }
    spdlog::info("Config Parse: fFramerateCap: {}", fFramerateCap);

    inipp::get_value(ini.sections["Shadow Quality"], "Resolution", iShadowResolution);
    if (iShadowResolution < 64 || iShadowResolution > 16384) {
        iShadowResolution = std::clamp(iShadowResolution, 64, 16384);
        spdlog::warn("Config Parse: iShadowResolution value invalid, clamped to {}", iShadowResolution);
    }
    spdlog::info("Config Parse: iShadowResolution: {}", iShadowResolution);

    spdlog::info("----------");

    // Grab desktop resolution
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();
    if (iCustomResX == 0 && iCustomResY == 0) {
        iCustomResX = DesktopDimensions.first;
        iCustomResY = DesktopDimensions.second;
        spdlog::info("Config Parse: Using desktop resolution of {}x{} as custom resolution.", iCustomResX, iCustomResY);
    }

    // Calculate aspect ratio
    iCurrentResX = iCustomResX;
    iCurrentResY = iCustomResY;
    CalculateAspectRatio(true);
}

WNDPROC OldWndProc;
LRESULT __stdcall NewWndProc(HWND window, UINT message_type, WPARAM w_param, LPARAM l_param) {
    switch (message_type) {
    case WM_CLOSE:
        // No exit/ALT+F4 handler bullshit.
        return DefWindowProc(window, message_type, w_param, l_param);
    }

    return CallWindowProc(OldWndProc, window, message_type, w_param, l_param);
};

SafetyHookInline SetWindowLongA_sh{};
LONG WINAPI SetWindowLongA_hk(HWND hWnd, int nIndex, LONG dwNewLong) {
    // Get window class name
    char sClassName[256] = { 0 };
    GetClassNameA(hWnd, sClassName, sizeof(sClassName));

    // Only modify game class
    if (std::string(sClassName) == std::string(sWindowClassName)) {
        // Set new wnd proc to kill alt+f4 handler
        if (OldWndProc == nullptr)
            OldWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)NewWndProc);

        if (nIndex == GWL_STYLE && bBorderlessMode) {
            // Modify GWL_STYLE
            dwNewLong &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);

            // Modify GW_EXSTYLE
            LONG dwExStyle = GetWindowLongA(hWnd, GWL_EXSTYLE);
            dwExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE);
            SetWindowLongA_sh.stdcall<LONG>(hWnd, GWL_EXSTYLE, dwExStyle);

            spdlog::info("Game Window: SetWindowLongA: Applied borderless styles.");

            // Hide the mouse cursor
            ShowCursor(FALSE);

            // Set new GWL_STYLE
            return SetWindowLongA_sh.stdcall<LONG>(hWnd, GWL_STYLE, dwNewLong);
        }
    }

    // Call the original function
    return SetWindowLongA_sh.stdcall<LONG>(hWnd, nIndex, dwNewLong);
}

void WindowManagement()
{
    // Hook SetWindowLongA
    HMODULE user32Module = GetModuleHandleW(L"user32.dll");
    if (user32Module) {
        FARPROC SetWindowLongA_fn = GetProcAddress(user32Module, "SetWindowLongA");
        if (SetWindowLongA_fn) {
            SetWindowLongA_sh = safetyhook::create_inline(SetWindowLongA_fn, reinterpret_cast<void*>(SetWindowLongA_hk));
            spdlog::info("Game Window: Hooked SetWindowLongA.");
        }
        else {
            spdlog::error("Game Window: Failed to get function address for SetWindowLongA.");
        }
    }
    else {
        spdlog::error("Game Window: Failed to get module handle for user32.dll.");
    }
}

void Resolution()
{
    if (bCustomRes) {
        // Add custom resolution
        uint8_t* ResolutionListScanResult = Memory::PatternScan(baseModule, "4C ?? ?? ?? ?? ?? ?? 41 ?? ?? 41 ?? ?? 45 ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? ??");
        uint8_t* ResolutionIndexScanResult = Memory::PatternScan(baseModule, "83 ?? 0F 0F ?? ?? 89 ?? ?? ?? ?? ?? C3");
        if (ResolutionListScanResult && ResolutionIndexScanResult) {
            spdlog::info("Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionListScanResult - (uintptr_t)baseModule);
            uintptr_t ResListAddr = Memory::GetAbsolute((uintptr_t)ResolutionListScanResult + 0x3);
            spdlog::info("Resolution: Resolution list address is {:s}+{:x}", sExeName.c_str(), ResListAddr - (uintptr_t)baseModule);
         
            // Write new resolution
            Memory::Write(ResListAddr + 0x6, (short)iCustomResX);
            Memory::Write(ResListAddr + 0x8, (short)iCustomResY);
            Memory::Write(ResListAddr + 0xA, (short)iCustomResY);
            spdlog::info("Resolution: Replaced {}x{} with {}x{}", 800, 450, (short)iCustomResX, (short)iCustomResY);

            spdlog::info("Resolution: Index address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionIndexScanResult - (uintptr_t)baseModule);
            uintptr_t ResIndexAddr = Memory::GetAbsolute((uintptr_t)ResolutionIndexScanResult - 0x4);
            spdlog::info("Resolution: Resolution index address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResIndexAddr - (uintptr_t)baseModule);

            // Force 800x450 on startup
            *reinterpret_cast<int*>(ResIndexAddr) = 1;

            static SafetyHookMid ForceResMidHook{};
            ForceResMidHook = safetyhook::create_mid(ResolutionIndexScanResult + 0x6,
                [](SafetyHookContext& ctx) {
                    // Force 800x450 on any resolution change
                    ctx.rcx = 1;
                });
        }
        else if (!ResolutionListScanResult || !ResolutionIndexScanResult) {
            spdlog::error("Resolution Fix: Pattern scan failed.");
        }

        // Spoof GetSystemMetrics results
        uint8_t* SystemMetrics1ScanResult = Memory::PatternScan(baseModule, "B9 01 00 00 00 41 ?? ?? 99 2B ?? D1 ?? 8B ??");
        uint8_t* SystemMetrics2ScanResult = Memory::PatternScan(baseModule, "0F ?? ?? 3B ?? 7C ?? B9 01 00 00 00 FF ?? ?? ?? ?? ?? 0F ?? ?? ?? 3B ?? 7D ?? 33 ??");
        uint8_t* ResCheckScanResult = Memory::PatternScan(baseModule, "74 ?? 33 ?? FF ?? ?? ?? ?? ?? 0F ?? ?? ?? 3B ?? 7C ??");
        if (SystemMetrics1ScanResult && SystemMetrics2ScanResult) {
            spdlog::info("SystemMetrics: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)SystemMetrics1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid WindowWidthMidHook{};
            WindowWidthMidHook = safetyhook::create_mid(SystemMetrics1ScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rax = INT_MAX;
                });

            static SafetyHookMid WindowHeightMidHook{};
            WindowHeightMidHook = safetyhook::create_mid(SystemMetrics1ScanResult + 0x15,
                [](SafetyHookContext& ctx) {
                    ctx.rax = INT_MAX;
                });

            spdlog::info("SystemMetrics: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)SystemMetrics2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid sysWidthMidHook{};
            sysWidthMidHook = safetyhook::create_mid(SystemMetrics2ScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rax = INT_MAX;
                });

            static SafetyHookMid sysHeightMidHook{};
            sysHeightMidHook = safetyhook::create_mid(SystemMetrics2ScanResult + 0x12,
                [](SafetyHookContext& ctx) {
                    ctx.rax = INT_MAX;
                });

            spdlog::info("SystemMetrics: ResCheck: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResCheckScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)ResCheckScanResult, "\xEB", 1);

            spdlog::info("SystemMetrics: ResCheck: Patched instruction.");
        }
        else if (!SystemMetrics1ScanResult || !SystemMetrics2ScanResult) {
            spdlog::error("SystemMetrics: Pattern scan(s) failed.");
        }

        // Window mode
        uint8_t* WindowModeScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? ?? 48 ?? ?? 83 ?? 02 0F 83 ?? ?? ?? ?? 83 ?? 01");
        if (WindowModeScanResult) {
            spdlog::info("Window Mode: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)WindowModeScanResult - (uintptr_t)baseModule);
            uintptr_t iWindowModeAddr = Memory::GetAbsolute((uintptr_t)WindowModeScanResult + 0x2);
            spdlog::info("Window Mode: iWindowMode address is {:s}+{:x}", sExeName.c_str(), iWindowModeAddr - (uintptr_t)baseModule);

            if (bBorderlessMode)
                bWindowedMode = true; // Force windowed mode if using borderless

            if (iWindowModeAddr)
                Memory::Write(iWindowModeAddr, (int)bWindowedMode);
        }
        else if (!WindowModeScanResult) {
            spdlog::error("Window Mode: Pattern scan failed.");
        }
    }
}

void AspectFOV()
{
    if (bFixAspect) {
        // Aspect ratio
        uint8_t* AspectRatioScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? ?? C6 ?? ?? ?? ?? ?? 01 89 ?? ?? ?? ?? ?? 40 ?? ?? ?? ?? ?? ?? 75 ??");
        if (AspectRatioScanResult) {
            spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)AspectRatioScanResult - (uintptr_t)baseModule);
            static SafetyHookMid AspectRatioMidHook{};
            AspectRatioMidHook = safetyhook::create_mid(AspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rbx + 0x1B0) {
                        *reinterpret_cast<float*>(ctx.rbx + 0x1B0) = fAspectRatio;
                    }
                });
        }
        else if (!AspectRatioScanResult) {
            spdlog::error("Aspect Ratio: Pattern scan failed.");
        }

        // Menu Aspect Ratio
        uint8_t* MenuAspectRatioScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? 4C ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ??");
        if (MenuAspectRatioScanResult) {
            spdlog::info("Menu Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MenuAspectRatioScanResult - (uintptr_t)baseModule);
            static SafetyHookMid AspectRatioMidHook{};
            AspectRatioMidHook = safetyhook::create_mid(MenuAspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm0.f32[0] = fAspectRatio;
                });
        }
        else if (!MenuAspectRatioScanResult) {
            spdlog::error("Menu Aspect Ratio: Pattern scan failed.");
        }
    }

    if (fGameplayFOVMulti != 1.00f) {
        // Gameplay FOV
        uint8_t* GameplayFOVScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 ?? ??");
        if (GameplayFOVScanResult) {
            spdlog::info("Gameplay FOV: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameplayFOVScanResult - (uintptr_t)baseModule);
            static SafetyHookMid AspectRatioMidHook{};
            AspectRatioMidHook = safetyhook::create_mid(GameplayFOVScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm0.f32[0] *= fGameplayFOVMulti;
                });
        }
        else if (!GameplayFOVScanResult) {
            spdlog::error("Gameplay FOV: Pattern scan failed.");
        }
    }
}

void HUD()
{
    // TODO: HUD BUGS
    // Some movies show visual errors at the beginning.
    // Notable enemy names are stretched.

    if (bFixHUD) {
        // HUD Size
        uint8_t* HUDSizeScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? 8B ?? ?? ?? 89 ?? ??");
        if (HUDSizeScanResult) {
            spdlog::info("HUD: Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDSizeScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDWidthMidHook{};
            HUDWidthMidHook = safetyhook::create_mid(HUDSizeScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDWidth;
                });

            static SafetyHookMid HUDHeightMidHook{};
            HUDHeightMidHook = safetyhook::create_mid(HUDSizeScanResult - 0x23,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm1.f32[0] = fHUDHeight;
                });
        }
        else if (!HUDSizeScanResult) {
            spdlog::error("HUD: Size: Pattern scan failed.");
        }

        // HUD Offset
        uint8_t* HUDOffsetCodepathScanResult = Memory::PatternScan(baseModule, "7A ?? 75 ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 7A ?? 74 ?? 48 ?? ?? ?? ?? ?? ?? 00 74 ??");
        uint8_t* HUDOffsetScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? 0F ?? ?? ?? 42 ?? ?? ?? ??");
        if (HUDOffsetCodepathScanResult && HUDOffsetScanResult) {
            spdlog::info("HUD: Offset: Codepath address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetCodepathScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)HUDOffsetCodepathScanResult, "\xEB", 1);
            spdlog::info("HUD: Offset: Patched instruction.");

            spdlog::info("HUD: Offset: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDWidthOffsetMidHook{};
            HUDWidthOffsetMidHook = safetyhook::create_mid(HUDOffsetScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = -(fNativeAspect / fAspectRatio);
                });

            static SafetyHookMid HUDHeightOffsetMidHook{};
            HUDHeightOffsetMidHook = safetyhook::create_mid(HUDOffsetScanResult + 0xD,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm1.f32[0] = fAspectMultiplier;
                });
        }
        else if (!HUDOffsetCodepathScanResult || !HUDOffsetScanResult) {
            spdlog::error("HUD: Offset: Pattern scan(s) failed.");
        }

        // Movies
        uint8_t* MoviesScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? 00 00 00 00 0F ?? ??");
        if (MoviesScanResult) {
            spdlog::info("HUD: Movies: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MoviesScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MovieWidthMidHook{};
            MovieWidthMidHook = safetyhook::create_mid(MoviesScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDWidth;
                });

            static SafetyHookMid MovieHeightMidHook{};
            MovieHeightMidHook = safetyhook::create_mid(MoviesScanResult + 0x18,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm1.f32[0] = fHUDHeight;
                });
        }
        else if (!MoviesScanResult) {
            spdlog::error("HUD: Movies: Pattern scan failed.");
        }

        // Fades
        uint8_t* FadesScanResult = Memory::PatternScan(baseModule, "66 0F ?? ?? ?? F3 0F ?? ?? ?? F3 0F ?? ?? ?? 0F ?? ?? F3 0F ?? ?? ?? F3 0F ?? ?? ??");
        if (FadesScanResult) {
            spdlog::info("HUD: Fades: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FadesScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FadeWidthMidHook{};
            FadeWidthMidHook = safetyhook::create_mid(FadesScanResult + 0x5,
                [](SafetyHookContext& ctx) {
                    if (ctx.xmm2.f32[0] == 1920.00f) {
                        if (fAspectRatio > fNativeAspect) {
                            ctx.xmm0.f32[0] = -(((1080.00f * fAspectRatio) - 1920.00f) / 2.00f);
                            ctx.xmm2.f32[0] = 1080.00f * fAspectRatio;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            ctx.xmm1.f32[0] = -(((1920.00f / fAspectRatio) - 1080.00f) / 2.00f);
                        }
                    }
                });

            static SafetyHookMid FadeHeightMidHook{};
            FadeHeightMidHook = safetyhook::create_mid(FadesScanResult + 0x12,
                [](SafetyHookContext& ctx) {
                    if (ctx.xmm2.f32[0] == 1920.00f) {
                        if (fAspectRatio < fNativeAspect)
                            ctx.xmm3.f32[0] = 1920.00f / fAspectRatio;                    
                    }
                });
        }
        else if (!FadesScanResult) {
            spdlog::error("HUD: Fades: Pattern scan failed.");
        }

        // Pause background
        uint8_t* PauseCaptureScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? ?? 00 00 87 44 F3 0F ?? ?? ?? ?? 44 ?? ?? ?? ?? ?? ?? ?? 4C ?? ?? ?? ??");
        uint8_t* PauseBGScanResult = Memory::PatternScan(baseModule, "D2 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? F3 0F ?? ?? ??");
        if (PauseCaptureScanResult && PauseBGScanResult) {
            spdlog::info("HUD: Pause Screen: Capture: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)PauseCaptureScanResult - (uintptr_t)baseModule);
            static SafetyHookMid PauseCaptureMidHook{};
            PauseCaptureMidHook = safetyhook::create_mid(PauseCaptureScanResult + 0x8,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x50) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x50) = (1080.00f * fAspectRatio) - fWidthOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x48) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x54) = (1920.00f / fAspectRatio) - fHeightOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x4C) = -fHeightOffset;
                        }
                    }
                });

            spdlog::info("HUD: Pause Screen: Background: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)PauseCaptureScanResult - (uintptr_t)baseModule);
            static SafetyHookMid PauseBGMidHook{};
            PauseBGMidHook = safetyhook::create_mid(PauseBGScanResult + 0x21,
                [](SafetyHookContext& ctx) {
                    if (ctx.rcx + 0x20 && ctx.xmm1.f32[0] == 1920.00f)
                    {
                        if (fAspectRatio > fNativeAspect) {
                            ctx.xmm1.f32[0] = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rcx + 0x20) = -((ctx.xmm1.f32[0] - 1920.00f) / 2.00f);
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            ctx.xmm0.f32[0] = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rcx + 0x24) = -((ctx.xmm0.f32[0] - 1080.00f) / 2.00f);
                        }
                    }
                });
        }
        else if (!PauseCaptureScanResult || !PauseBGScanResult) {
            spdlog::error("HUD: Pause Screen: Pattern scan(s) failed.");
        }

        // Mission select
        uint8_t* MissionSelectCaptureScanResult = Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? ?? 45 ?? ?? BA 01 00 00 00 E8 ?? ?? ?? ??");
        uint8_t* MissionSelectBGScanResult = Memory::PatternScan(baseModule, "48 ?? ?? ?? 49 ?? ?? ?? 4C ?? ?? ?? 4C ?? ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? ?? ?? 48 ?? ?? ?? 5F C3");
        if (MissionSelectCaptureScanResult && MissionSelectBGScanResult) {
            spdlog::info("HUD: Mission Select Screen: Capture: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MissionSelectCaptureScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MissionSelectCaptureMidHook{};
            MissionSelectCaptureMidHook = safetyhook::create_mid(MissionSelectCaptureScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x40 && ctx.xmm0.f32[0] == 1920.00f) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x48) = (1080.00f * fAspectRatio) - fWidthOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x40) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x4C) = (1920.00f / fAspectRatio) - fHeightOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x44) = -fHeightOffset;
                        }
                    }
                });

            spdlog::info("HUD: ission Select Screen: Background: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MissionSelectBGScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MissionSelectBGMidHook{};
            MissionSelectBGMidHook = safetyhook::create_mid(MissionSelectBGScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.r8 + 0x20 && ctx.xmm1.f32[0] == 1920.00f) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.r8 + 0x28) = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.r8 + 0x20) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.r8 + 0x2C) = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.r8 + 0x24) = -fHeightOffset;
                        }
                    }
                });
        }
        else if (!MissionSelectCaptureScanResult || !MissionSelectBGScanResult) {
            spdlog::error("HUD: MissionSelect Screen: Pattern scan(s) failed.");
        }

        // Menu Backgrounds
        uint8_t* MenuBackgroundsScanResult = Memory::PatternScan(baseModule, "7E ?? 49 ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 4C ?? ?? ?? ?? 4C ?? ?? ?? ??");
        if (MenuBackgroundsScanResult) {
            spdlog::info("HUD: Backgrounds: Menu: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MenuBackgroundsScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MenuBackgroundsMidHook{};
            MenuBackgroundsMidHook = safetyhook::create_mid(MenuBackgroundsScanResult + 0x2,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x50 && ctx.xmm0.f32[0] == (float)iCustomResX && ctx.xmm1.f32[0] == (float)iCustomResY) {
                        if (fAspectRatio > fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.rsp + 0x58) = fHUDWidth + fHUDWidthOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x50) = fHUDWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.rsp + 0x5C) = fHUDHeight + fHUDHeightOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x54) = fHUDHeightOffset;
                        }
                    }
                });
        }
        else if (!MenuBackgroundsScanResult) {
            spdlog::error("HUD: Menu Backgrounds: Pattern scan failed.");
        }

        // HUD Backgrounds
        uint8_t* HUDBackgrounds1ScanResult = Memory::PatternScan(baseModule, "8B ?? 89 ?? ?? 48 8B ?? ?? 48 89 ?? ?? 48 89 ?? ?? 48 89 ?? ??");
        uint8_t* HUDBackgrounds2ScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 0F 84 ?? ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ?? 33 ?? 83 ?? ?? ?? ?? ?? 03");
        uint8_t* HUDBackgrounds3ScanResult = Memory::PatternScan(baseModule, "48 8B ?? ?? 48 89 ?? ?? 48 89 ?? ?? 48 89 ?? ?? 83 ?? ?? ?? ?? ?? 00 74 ??");
        uint8_t* HUDBackgrounds4ScanResult = Memory::PatternScan(baseModule, "48 ?? ?? ?? 89 ?? ?? 44 0F ?? ?? ?? ?? 48 ?? ?? ?? 48 ?? ?? ?? 48 ?? ?? ?? 48 ?? ?? ??");
        uint8_t* HUDBackgrounds5ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? 85 ?? 74 ?? FF ?? 74 ?? FF ?? 75 ??");
        if (HUDBackgrounds1ScanResult && HUDBackgrounds2ScanResult && HUDBackgrounds3ScanResult && HUDBackgrounds4ScanResult && HUDBackgrounds5ScanResult) {
            spdlog::info("HUD: Backgrounds: Other 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgrounds1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDBackgrounds1MidHook{};
            HUDBackgrounds1MidHook = safetyhook::create_mid(HUDBackgrounds1ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rdx + 0x20) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rdx + 0x28) = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdx + 0x20) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rdx + 0x2C) = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdx + 0x24) = -fHeightOffset;
                        }
                    }
                });

            spdlog::info("HUD: Backgrounds: Other 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgrounds2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDBackgrounds2MidHook{};
            HUDBackgrounds2MidHook = safetyhook::create_mid(HUDBackgrounds2ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x40) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x48) = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rsp + 0x40) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x4C) = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rsp + 0x44) = -fHeightOffset;
                        }
                    }
                });

            spdlog::info("HUD: Backgrounds: Other 3: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgrounds3ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDBackgrounds3MidHook{};
            HUDBackgrounds3MidHook = safetyhook::create_mid(HUDBackgrounds3ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rdx + 0x20) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rdx + 0x28) = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdx + 0x20) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rdx + 0x2C) = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdx + 0x24) = -fHeightOffset;
                        }
                    }
                });

            spdlog::info("HUD: Backgrounds: Other 4: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgrounds4ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDBackgrounds4MidHook{};
            HUDBackgrounds4MidHook = safetyhook::create_mid(HUDBackgrounds4ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rdx + 0x20) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rdx + 0x28) = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdx + 0x20) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rdx + 0x2C) = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rdx + 0x24) = -fHeightOffset;
                        }
                    }
                });

            spdlog::info("HUD: Backgrounds: Other 5: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgrounds5ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDBackgrounds5MidHook{};
            HUDBackgrounds5MidHook = safetyhook::create_mid(HUDBackgrounds5ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x30) {
                        if (fAspectRatio > fNativeAspect) {
                            float fWidthOffset = ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x38) = 1080.00f * fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rsp + 0x30) = -fWidthOffset;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            float fHeightOffset = ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                            *reinterpret_cast<float*>(ctx.rsp + 0x3C) = 1920.00f / fAspectRatio;
                            *reinterpret_cast<float*>(ctx.rsp + 0x34) = -fHeightOffset;
                        }
                    }
                });
        }
        else if (!HUDBackgrounds1ScanResult || !HUDBackgrounds2ScanResult || !HUDBackgrounds3ScanResult || !HUDBackgrounds4ScanResult || !HUDBackgrounds5ScanResult) {
            spdlog::error("HUD: Backgrounds: Pattern scan(s) failed.");
        }
    }   
}

void Framerate()
{
    if (fFramerateCap != 60.00f) {
        // Framerate Cap
        uint8_t* FramerateCapScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? 0F ?? ?? 0F ?? ?? 76 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 ?? ?? ?? ?? 83 ?? 01");
        if (FramerateCapScanResult) {
            spdlog::info("Framerate: Cap: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FramerateCapScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FramerateCapMidHook{};
            FramerateCapMidHook = safetyhook::create_mid(FramerateCapScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm1.f32[0] = 1.00f / fFramerateCap;
                });
        }
        else if (!FramerateCapScanResult) {
            spdlog::error("Framerate: Cap: Pattern scan failed.");
        }

        // Game Speed
        uint8_t* GameSpeedScanResult = Memory::PatternScan(baseModule, "0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 66 0F ?? ?? ?? ?? ?? ?? 66 0F ?? ?? 0F ?? ?? 72 ??");
        if (GameSpeedScanResult) {
            spdlog::info("Framerate: Game Speed: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameSpeedScanResult - (uintptr_t)baseModule);
            static SafetyHookMid GameSpeedMidHook{};
            GameSpeedMidHook = safetyhook::create_mid(GameSpeedScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm3.f32[0] = 1.00f / fFramerateCap;
                });
        }
        else if (!GameSpeedScanResult) {
            spdlog::error("Framerate: Game Speed: Pattern scan failed.");
        }

        // Get current frametime
        uint8_t* CurrentFrametimeScanResult = Memory::PatternScan(baseModule, "66 0F ?? ?? ?? ?? ?? ?? 66 0F ?? ?? 0F ?? ?? 72 ?? F3 0F ?? ?? ??");
        if (CurrentFrametimeScanResult) {
            spdlog::info("Framerate: Frametime: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CurrentFrametimeScanResult - (uintptr_t)baseModule);
            static SafetyHookMid CurrentFrametimeMidHook{};
            CurrentFrametimeMidHook = safetyhook::create_mid(CurrentFrametimeScanResult,
                [](SafetyHookContext& ctx) {
                    fCurrentFrametime = ctx.xmm4.f32[0];
                });
        }
        else if (!CurrentFrametimeScanResult) {
            spdlog::error("Framerate: Frametime: Pattern scan failed.");
        }

        // Input Speed
        uint8_t* ControllerInputSpeedScanResult = Memory::PatternScan(baseModule, "41 0F ?? ?? 41 ?? ?? 41 ?? ?? 3C ?? 72 ?? 8B ?? 09 ?? ??");
        uint8_t* KeyboardInputSpeedScanResult = Memory::PatternScan(baseModule, "F3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 41 ?? ?? 48 ?? ?? ?? ?? ?? ?? 8B ?? 85 ?? 74 ?? FF ??");
        if (ControllerInputSpeedScanResult && KeyboardInputSpeedScanResult) {
            spdlog::info("Framerate: Input Speed: Controller: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ControllerInputSpeedScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ControllerInputSpeedMidHook{};

            static uintptr_t Target1Addr = (uintptr_t)ControllerInputSpeedScanResult + 0x16;
            static uintptr_t Target2Addr = (uintptr_t)ControllerInputSpeedScanResult + 0x1A;

            ControllerInputSpeedMidHook = safetyhook::create_mid(ControllerInputSpeedScanResult + 0xC,
                [](SafetyHookContext& ctx) {
                    // Get current count
                    int iCurrentCount = (int)ctx.rax;

                    // Get current framerate
                    int iCurrentFramerate = static_cast<int>(1.00f / fCurrentFrametime);

                    // Calculate new target count by assuming it is 20 for 60fps
                    int iTarget = static_cast<int>(20.00f * ((float)iCurrentFramerate / 60.00f));

                    // Alter other targets
                    if (Target1Addr && Target2Addr) {
                        int iAltTarget = static_cast<int>(16.00f * ((float)iCurrentFramerate / 60.00f));
                        Memory::Write((uintptr_t)Target1Addr, (BYTE)iAltTarget);
                        Memory::Write((uintptr_t)Target2Addr, (BYTE)iAltTarget);
                    }

                    // Check if current count exceeds the target
                    if (iCurrentCount < iTarget)
                        ctx.rflags |= (1 << 0);     // Set CF
                    else
                        ctx.rflags &= ~(1 << 0);    // Clear CF
                });

            spdlog::info("Framerate: Input Speed: Keyboard: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)KeyboardInputSpeedScanResult - (uintptr_t)baseModule);
            static SafetyHookMid KeyboardInputSpeedMidHook{};
            KeyboardInputSpeedMidHook = safetyhook::create_mid(KeyboardInputSpeedScanResult + 0x5,
                [](SafetyHookContext& ctx) {
                    ctx.xmm1.f32[0] = 1.00f / ((1.00f / fCurrentFrametime) / 60.00f);
                });
        }
        else if (!ControllerInputSpeedScanResult || !KeyboardInputSpeedScanResult) {
            spdlog::error("Framerate: Input Speed: Pattern scan(s) failed.");
        }
    }
}

void Misc()
{
    // Disable Windows 7 compatibility message on startup
    uint8_t* WindowsCompatibilityMessageScanResult = Memory::PatternScan(baseModule, "85 ?? 0F 84 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? 00 75 ?? 48 ?? ?? ?? ?? ?? ?? 33 ??");
    if (WindowsCompatibilityMessageScanResult) {
        spdlog::info("Windows Compatibility Message: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)WindowsCompatibilityMessageScanResult - (uintptr_t)baseModule);
        static SafetyHookMid WinCompCheckMidHook{};
        WinCompCheckMidHook = safetyhook::create_mid(WindowsCompatibilityMessageScanResult,
            [](SafetyHookContext& ctx) {
                ctx.rax = 0;
            });
    }
    else if (!WindowsCompatibilityMessageScanResult) {
        spdlog::error("Windows Compatibility Message: Pattern scan failed.");
    }

    if (iShadowResolution != 4096) {
        // Shadow Quality 
        uint8_t* ShadowQualityScanResult = Memory::PatternScan(baseModule, "C6 ?? ?? ?? ?? 33 ?? 41 ?? 01 00 00 00 89 ?? ?? ??");
        if (ShadowQualityScanResult) {
            spdlog::info("Shadow Quality: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowQualityScanResult - (uintptr_t)baseModule);
            static SafetyHookMid WinCompCheckMidHook{};
            WinCompCheckMidHook = safetyhook::create_mid(ShadowQualityScanResult,
                [](SafetyHookContext& ctx) {
                    // If shadows are set to high
                    if (ctx.rax == 0x1000) {
                        // Change shadow resolution
                        ctx.rax = iShadowResolution;
                        ctx.rdx = iShadowResolution;
                    }

                });
        }
        else if (!ShadowQualityScanResult) {
            spdlog::error("Shadow Quality: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    WindowManagement();
    Resolution();
    AspectFOV();
    HUD();
    Framerate();
    Misc();
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        thisModule = hModule;
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, CREATE_SUSPENDED, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_TIME_CRITICAL);
            ResumeThread(mainHandle);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}