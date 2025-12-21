// Expected.h
#pragma once
#include "CoreMinimal.h"
#include "Common/Error.h"

namespace rinrin::uejs
{
    // Tag type for error (only errors need wrapping)
    struct Unexpected
    {
        FError Error;
        explicit Unexpected(FError InError) : Error(MoveTemp(InError)) {}
    };

    // Helper function - short and clear
    inline Unexpected Err(FError Error)
    {
        return Unexpected(MoveTemp(Error));
    }

    template <class T>
    class TExpected
    {
    public:
        // Values can be implicitly constructed
        TExpected(const T &v) : V(TInPlaceType<T>(), v) {}
        TExpected(T &&v) : V(TInPlaceType<T>(), MoveTemp(v)) {}

        // Errors must be wrapped with Err()
        TExpected(Unexpected e) : V(TInPlaceType<FError>(), MoveTemp(e.Error)) {}

        bool HasValue() const { return V.template IsType<T>(); }
        explicit operator bool() const { return HasValue(); }

        T &Value()
        {
            check(HasValue());
            return V.template Get<T>();
        }
        const T &Value() const
        {
            check(HasValue());
            return V.template Get<T>();
        }

        FError &Error()
        {
            check(!HasValue());
            return V.template Get<FError>();
        }
        const FError &Error() const
        {
            check(!HasValue());
            return V.template Get<FError>();
        }

        T &operator*() { return Value(); }
        const T &operator*() const { return Value(); }

    private:
        TVariant<T, FError> V;
    };
}
