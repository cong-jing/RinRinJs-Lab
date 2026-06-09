// Expected.h
#pragma once
#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "Util/Error.h"

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
    class [[nodiscard]] TExpected
    {
    public:
        // Values can be implicitly constructed
        TExpected(const T &v) : V(TInPlaceType<T>(), v) {}
        TExpected(T &&v) : V(TInPlaceType<T>(), MoveTemp(v)) {}

        // Errors must be wrapped with Err()
        TExpected(Unexpected e) : V(TInPlaceType<FError>(), MoveTemp(e.Error)) {}

        bool HasValue() const { return V.template IsType<T>(); }
        bool HasError() const { return V.template IsType<FError>(); }
        explicit operator bool() const { return !HasError(); }

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

        // Move-out error (rvalue-only) for zero-copy propagation
        FError TakeError() &&
        {
            check(HasError());
            return MoveTemp(V.template Get<FError>());
        }

        T &operator*() { return Value(); }
        const T &operator*() const { return Value(); }

    private:
        TVariant<T, FError> V;
    };

    // Specialization for void
    template <>
    class [[nodiscard]] TExpected<void>
    {
    public:
        // Default constructor for success
        TExpected() : bHasError(false) {}

        // Errors must be wrapped with Err()
        TExpected(Unexpected e) : ErrorValue(MoveTemp(e.Error)), bHasError(true) {}

        bool HasValue() const { return !bHasError; }
        bool HasError() const { return bHasError; }
        explicit operator bool() const { return !HasError(); }

        void Value() const
        {
            check(HasValue());
        }

        FError &Error()
        {
            check(HasError());
            return ErrorValue;
        }
        const FError &Error() const
        {
            check(HasError());
            return ErrorValue;
        }

        // Move-out error (rvalue-only) for zero-copy propagation
        FError TakeError() &&
        {
            check(HasError());
            return MoveTemp(ErrorValue);
        }

    private:
        FError ErrorValue;
        bool bHasError;
    };
}
