// Value/ValueIntoJs.h
#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include <string>

namespace rinrin::uejs
{
    // Tag types to distinguish undefined vs null (JS has both).
    struct FJsUndefinedTag
    {
    };

    struct FJsNullTag
    {
    };

    /**
     * FValueIntoJs
     * - Public, engine-stable value container used to pass values INTO JS.
     * - Does NOT include or reference any V8 types.
     * - Keep this type small and predictable; conversion to V8 happens at the boundary in Private code.
     */
    class RINRINJS_API FValueIntoJs
    {
    public:
        enum class EType : uint8
        {
            Undefined,
            Null,
            Bool,
            Int32,
            Double,
            String,
            // Optional (add later if needed):
            // UObject,
            // Array,
            // Object,
        };

        using FStorage = TVariant<
            FJsUndefinedTag, // 0
            FJsNullTag,      // 1
            bool,            // 2
            int32,           // 3
            double,          // 4
            std::string      // 5
            >;

        // Default: undefined
        FValueIntoJs();

        // Factory helpers
        static FValueIntoJs Undefined();
        static FValueIntoJs Null();
        static FValueIntoJs FromBool(bool b);
        static FValueIntoJs FromInt32(int32 v);
        static FValueIntoJs FromDouble(double v);
        static FValueIntoJs FromString(const FString &s);
        static FValueIntoJs FromString(FString &&s);
        static FValueIntoJs FromString(const std::string &s);
        static FValueIntoJs FromString(std::string &&s);

        // Type query
        EType GetType() const;

        bool IsUndefined() const;
        bool IsNull() const;
        bool IsBool() const;
        bool IsInt32() const;
        bool IsDouble() const;
        bool IsString() const;

        // Getters (caller should check type first; these assert in debug if mismatched)
        bool AsBoolChecked() const;
        int32 AsInt32Checked() const;
        double AsDoubleChecked() const;
        const std::string &AsStringChecked() const;

    private:
        explicit FValueIntoJs(FStorage &&InStorage);

        FStorage Storage;
    };
} // namespace rinrin::uejs
