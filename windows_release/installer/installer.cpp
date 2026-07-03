#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "payload_data.h"

namespace fs = std::filesystem;

namespace {

std::wstring envVar(const wchar_t* name, const wchar_t* fallback)
{
    wchar_t buffer[32768];
    DWORD size = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    if (size == 0 || size >= std::size(buffer)) {
        return fallback;
    }
    return std::wstring(buffer, size);
}

bool writeFile(const fs::path& path, const unsigned char* data, std::size_t size, std::wstring& error)
{
    try {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = L"Could not open file for writing: " + path.wstring();
            return false;
        }
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) {
            error = L"Could not write file: " + path.wstring();
            return false;
        }
        return true;
    } catch (const std::exception&) {
        error = L"Filesystem error while writing: " + path.wstring();
        return false;
    }
}

bool removeIfExists(const fs::path& path, std::wstring& error)
{
    try {
        if (fs::exists(path)) {
            fs::remove_all(path);
        }
        return true;
    } catch (const std::exception&) {
        error = L"Could not remove old install path: " + path.wstring();
        return false;
    }
}

void showMessage(const std::wstring& title, const std::wstring& body, UINT icon)
{
    MessageBoxW(nullptr, body.c_str(), title.c_str(), MB_OK | icon);
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const std::wstring commonFiles = envVar(L"CommonProgramFiles", L"C:\\Program Files\\Common Files");
    const std::wstring programData = envVar(L"ProgramData", L"C:\\ProgramData");

    const fs::path ofxRoot = fs::path(commonFiles) / L"OFX" / L"Plugins";
    const fs::path lutRoot = fs::path(programData) / L"Blackmagic Design" / L"DaVinci Resolve" / L"Support" / L"LUT";

    std::wstring error;
    if (!removeIfExists(ofxRoot / L"BuckswoodFakeDiagnostic.ofx.bundle", error) ||
        !removeIfExists(ofxRoot / L"BuckswoodLensPhysics.ofx.bundle", error) ||
        !removeIfExists(ofxRoot / L"BuckswoodAIPhotorealizer.ofx.bundle", error)) {
        showMessage(L"Buckswood Resolve Plugins", error, MB_ICONERROR);
        return 1;
    }

    struct InstallFile {
        fs::path base;
        const wchar_t* relative;
        const unsigned char* data;
        std::size_t size;
    };

    const std::vector<InstallFile> files = {
        {ofxRoot, L"BuckswoodFakeDiagnostic.ofx.bundle\\Contents\\Info.plist", payload::fake_info_plist, payload::fake_info_plist_size},
        {ofxRoot, L"BuckswoodFakeDiagnostic.ofx.bundle\\Contents\\Win64\\BuckswoodFakeDiagnostic.ofx", payload::fake_ofx, payload::fake_ofx_size},
        {ofxRoot, L"BuckswoodLensPhysics.ofx.bundle\\Contents\\Info.plist", payload::lens_info_plist, payload::lens_info_plist_size},
        {ofxRoot, L"BuckswoodLensPhysics.ofx.bundle\\Contents\\Win64\\BuckswoodLensPhysics.ofx", payload::lens_ofx, payload::lens_ofx_size},
        {ofxRoot, L"BuckswoodAIPhotorealizer.ofx.bundle\\Contents\\Info.plist", payload::photo_info_plist, payload::photo_info_plist_size},
        {ofxRoot, L"BuckswoodAIPhotorealizer.ofx.bundle\\Contents\\Win64\\BuckswoodAIPhotorealizer.ofx", payload::photo_ofx, payload::photo_ofx_size},
        {lutRoot, L"Buckswood_Lens_Physics_v01.dctl", payload::lens_dctl, payload::lens_dctl_size},
        {lutRoot, L"Buckswood_AI_Photorealizer_v01.dctl", payload::photo_dctl, payload::photo_dctl_size},
    };

    for (const InstallFile& file : files) {
        if (!writeFile(file.base / file.relative, file.data, file.size, error)) {
            showMessage(L"Buckswood Resolve Plugins", error, MB_ICONERROR);
            return 1;
        }
    }

    const fs::path logPath = fs::path(programData) / L"Buckswood" / L"ResolvePlugins" / L"install.log";
    const std::string log =
        "Buckswood Resolve Plugins installed.\r\n"
        "OFX: C:\\Program Files\\Common Files\\OFX\\Plugins\r\n"
        "DCTL: C:\\ProgramData\\Blackmagic Design\\DaVinci Resolve\\Support\\LUT\r\n";
    writeFile(logPath, reinterpret_cast<const unsigned char*>(log.data()), log.size(), error);

    showMessage(
        L"Buckswood Resolve Plugins",
        L"Installation complete.\n\nRestart DaVinci Resolve, then open:\n"
        L"Color Page > OpenFX > Buckswood Diagnostic\n"
        L"Color Page > OpenFX > Buckswood\n"
        L"Color Page > OpenFX > Buckswood AI\n\n"
        L"DCTL fallbacks are installed in the Resolve LUT folder.",
        MB_ICONINFORMATION);
    return 0;
}
