#include <windows.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "payload_data.h"

namespace fs = std::filesystem;

namespace {

std::wstring envVar(const wchar_t* name, const wchar_t* fallback)
{
    wchar_t buffer[32768];
    const DWORD size = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
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
            error = L"Could not open file for writing:\n" + path.wstring();
            return false;
        }

        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) {
            error = L"Could not write file:\n" + path.wstring();
            return false;
        }
        return true;
    } catch (const std::exception&) {
        error = L"Filesystem error while writing:\n" + path.wstring();
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
    const std::wstring programFiles = envVar(L"ProgramFiles", L"C:\\Program Files");
    const std::wstring programData = envVar(L"ProgramData", L"C:\\ProgramData");
    const fs::path pluginRoot = fs::path(programFiles) / L"Adobe" / L"Common" / L"Plugins" / L"7.0" / L"MediaCore" / L"Buckswood";

    struct InstallFile {
        const wchar_t* fileName;
        const unsigned char* data;
        std::size_t size;
    };

    const std::vector<InstallFile> files = {
        {L"BuckswoodAIPhotorealizer.prm", payload::photo_prm, payload::photo_prm_size},
        {L"BuckswoodLensPhysics.prm", payload::lens_prm, payload::lens_prm_size},
    };

    std::wstring error;
    for (const InstallFile& file : files) {
        if (!writeFile(pluginRoot / file.fileName, file.data, file.size, error)) {
            showMessage(L"Buckswood Premiere Plugins", error, MB_ICONERROR);
            return 1;
        }
    }

    const fs::path logPath = fs::path(programData) / L"Buckswood" / L"PremierePlugins" / L"install.log";
    const std::string log =
        "Buckswood Premiere Plugins installed.\r\n"
        "Plugin path: C:\\Program Files\\Adobe\\Common\\Plugins\\7.0\\MediaCore\\Buckswood\r\n";
    writeFile(logPath, reinterpret_cast<const unsigned char*>(log.data()), log.size(), error);

    showMessage(
        L"Buckswood Premiere Plugins",
        L"Installation complete.\n\nRestart Premiere Pro, then search for \"Buckswood\" in the Effects panel.",
        MB_ICONINFORMATION);
    return 0;
}
