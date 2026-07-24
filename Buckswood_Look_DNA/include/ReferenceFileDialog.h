#pragma once

#include <string>

namespace buckswood_lookdna {

bool openReferenceFileDialog(
    const std::string& initialPath,
    std::string& selectedPath);

} // namespace buckswood_lookdna
