#pragma once

#include "sleekpr/core/templates/PaperSpec.h"

#include <optional>

namespace sleekpr::core {

PaperSpec defaultLabel80x30PaperSpec();
std::optional<PaperSpec> builtInPaperSpec(const QString& paperSpecId);

} // namespace sleekpr::core
