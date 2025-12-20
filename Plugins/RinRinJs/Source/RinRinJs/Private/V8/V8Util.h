#pragma once

#include "CoreMinimal.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace rinrin::uejs
{

  class V8Util
  {
  public:
    static FString V8ToFString(v8::Isolate *Isolate, v8::Local<v8::Value> Value);
    static void LogTryCatch(v8::Isolate *Isolate, v8::TryCatch &TryCatch, const TCHAR *Prefix = TEXT("V8"));
  };

} // namespace rinrin::uejs
