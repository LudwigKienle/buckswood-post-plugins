#include "ReferenceFileDialog.h"

#if defined(_WIN32)

#include <windows.h>
#include <commdlg.h>

#include <array>
#include <filesystem>
#include <string>

namespace {

std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, result.data(), size);
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}

std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}

} // namespace

namespace buckswood_lookdna {

bool openReferenceFileDialog(
    const std::string& initialPath,
    std::string& selectedPath)
{
    std::array<wchar_t, 32768> pathBuffer{};
    std::wstring initialDirectory;
    const std::wstring initialWide = utf8ToWide(initialPath);
    if (!initialWide.empty()) {
        const std::filesystem::path path(initialWide);
        std::error_code error;
        const std::filesystem::path directory =
            std::filesystem::is_directory(path, error) ? path : path.parent_path();
        if (!error && !directory.empty()) {
            initialDirectory = directory.wstring();
        }
    }

    static const wchar_t filter[] =
        L"Look DNA references\0*.jpg;*.jpeg;*.png;*.tif;*.tiff;*.bmp;*.bwlook\0"
        L"Images\0*.jpg;*.jpeg;*.png;*.tif;*.tiff;*.bmp\0"
        L"BWLOOK profiles\0*.bwlook\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = pathBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(pathBuffer.size());
    dialog.lpstrInitialDir =
        initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    dialog.lpstrTitle = L"Choose a Look DNA reference";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;

    if (!GetOpenFileNameW(&dialog)) {
        return false;
    }

    selectedPath = wideToUtf8(pathBuffer.data());
    return !selectedPath.empty();
}

} // namespace buckswood_lookdna

#endif
