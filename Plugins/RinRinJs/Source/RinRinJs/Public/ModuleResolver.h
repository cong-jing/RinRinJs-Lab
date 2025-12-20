#pragma once

#include "CoreMinimal.h"
#include <string>
#include <functional>

namespace rinrin::uejs
{
    using FResolveModuleIdFn = std::function<bool(
        std::string_view ReferrerResolvedId,
        std::string_view RequestSpecifier,
        std::string &OutResolvedModuleId,
        std::string &OutError)>;

    using FLoadSourceByModuleIdFn = std::function<bool(
        std::string_view ResolvedModuleId,
        std::string &OutSourceUtf8,
        std::string &OutError)>;
}
