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
std::string sFixVer = "0.0.1";
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
bool bFixAspect;
bool bFixHUD;

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
    spdlog::info("Config Parse: bCustomRes: {}", bCustomRes);
    spdlog::info("Config Parse: iCustomResX: {}", iCustomResX);
    spdlog::info("Config Parse: iCustomResY: {}", iCustomResY);

    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);
    spdlog::info("Config Parse: bFixAspect: {}", bFixAspect);

    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bFixHUD);
    spdlog::info("Config Parse: bFixHUD: {}", bFixHUD);

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

void Resolution()
{
    if (bCustomRes) {
        // Add custom resolution
        uint8_t* ResolutionListScanResult = Memory::PatternScan(baseModule, "4C ?? ?? ?? ?? ?? ?? 41 ?? ?? 41 ?? ?? 45 ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? ??");
        uint8_t* ResolutionIndexScanResult = Memory::PatternScan(baseModule, "83 ?? 0F 0F ?? ?? 89 ?? ?? ?? ?? ?? C3");
        if (ResolutionListScanResult && ResolutionIndexScanResult) {
            spdlog::info("Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionListScanResult - (uintptr_t)baseModule);
            uintptr_t ResListAddr = Memory::GetAbsolute((uintptr_t)ResolutionListScanResult + 0x3);
            spdlog::info("Resolution: Resolution list address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResListAddr - (uintptr_t)baseModule);
         
            if (ResListAddr) {
                for (int i = 0; i < 44; i++) {
                    int offset = (i * 0x2);
                    short ResX = *reinterpret_cast<short*>(ResListAddr + offset);
                    short ResY = *reinterpret_cast<short*>(ResListAddr + offset + 0x2);

                    if (ResX == 2560 && ResY == 1440) {
                        Memory::Write((uintptr_t)ResListAddr + offset, (short)iCustomResX);
                        Memory::Write((uintptr_t)ResListAddr + offset + 0x2, (short)iCustomResY);
                        spdlog::info("Resolution: Replaced {}x{} with {}x{}", ResX, ResY, (short)iCustomResX, (short)iCustomResY);
                    }
                }
            }
            spdlog::info("Resolution: Index address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionIndexScanResult - (uintptr_t)baseModule);
            uintptr_t ResIndexAddr = Memory::GetAbsolute((uintptr_t)ResolutionIndexScanResult - 0x4);
            spdlog::info("Resolution: Resolution index address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResIndexAddr - (uintptr_t)baseModule);

            // Force 2560x1440 on startup
            *reinterpret_cast<int*>(ResIndexAddr) = 0xC;

            static SafetyHookMid ForceResMidHook{};
            ForceResMidHook = safetyhook::create_mid(ResolutionIndexScanResult + 0x6,
                [](SafetyHookContext& ctx) {
                    // Force 2560x1440 on any resolution change
                    ctx.rcx = 0xC;
                });
         
        }
        else if (!ResolutionListScanResult || !ResolutionIndexScanResult) {
            spdlog::error("Resolution Fix: Pattern scan failed.");
        }
    }
}

void AspectFOV()
{
    if (bFixAspect) {
        // Aspect ratio
        uint8_t* AspectRatioScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? 0F ?? ?? ?? ?? ?? ?? 74 ??");
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
}

void HUD()
{
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
        uint8_t* HUDOffsetScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? 0F ?? ?? ?? 42 ?? ?? ?? ??");
        if (HUDOffsetScanResult) {
            spdlog::info("HUD: Offset: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetScanResult - (uintptr_t)baseModule);

            static SafetyHookMid HUDWidthOffsetMidHook{};
            HUDWidthOffsetMidHook = safetyhook::create_mid(HUDOffsetScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = -(fNativeAspect / fAspectRatio);
                });

            static SafetyHookMid HUDHeightOffsetMidHook{};
            HUDHeightOffsetMidHook = safetyhook::create_mid(HUDOffsetScanResult - 0xF,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDHeight;
                });
        }
        else if (!HUDOffsetScanResult) {
            spdlog::error("HUD: Offset: Pattern scan failed.");
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
    }   
}

void Misc()
{
    // Disable Windows 7 compatibility message on startup
    uint8_t* WindowsCompatibilityMessageScanResult = Memory::PatternScan(baseModule, "85 ?? 0F 84 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? 00 75 ?? 48 ?? ?? ?? ?? ?? ?? 33 ?? ");
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
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    Resolution();
    AspectFOV();
    HUD();
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