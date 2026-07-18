#ifndef __UHANDERS_RESULT_H__
#define __UHANDERS_RESULT_H__

#include "macros.h"

#include <functional> // std::invoke
#include <type_traits>
#include <utility>

template <typename T>
concept ErrorLike = std::is_enum_v<T> &&
                    std::is_same_v<std::underlying_type_t<T>, u8> && requires {
                        { T::Success };
                        static_cast<u8>(T::Success) == 0;
                    };

template <typename T, ErrorLike Error> struct Result;

template <typename T, ErrorLike Error>
    requires(std::is_default_constructible_v<T> and
             !std::is_lvalue_reference_v<T> and
             !std::is_constructible_v<T, Error> and
             !std::is_constructible_v<Error, T>)
struct Result<T, Error>
{
  private:
    T m_value;
    Error m_error;

  public:
    [[nodiscard]] T &value() &
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return m_value;
    }
    [[nodiscard]] const T &value() const &
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return m_value;
    }
    [[nodiscard]] T &&value() &&
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return std::move(m_value);
    }

    [[nodiscard]] T &operator->() &
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return m_value;
    }
    [[nodiscard]] const T &operator->() const &
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return m_value;
    }
    [[nodiscard]] T &&operator->() &&
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return std::move(m_value);
    }

    [[nodiscard]] Error error() const { return m_error; }

    [[nodiscard]] constexpr bool isError() const
    {
        return m_error != Error::Success;
    }
    [[nodiscard]] constexpr bool isSuccess() const
    {
        return m_error == Error::Success;
    }

    constexpr explicit operator bool() const { return isSuccess(); }

    Result() = delete;

    constexpr Result(Error error) : m_value{}, m_error(error)
    {
        uassert(error != Error::Success,
                "Attempt to construct error with a success value, probably not "
                "what you wanted");
    }

    template <typename SingleArg>
        requires(!std::is_same_v<Error, SingleArg>)
    constexpr Result(SingleArg &&arg)
        : m_value(std::forward<SingleArg>(arg)), m_error(Error::Success)
    {
    }

    template <typename SingleArg, typename... Args>
    constexpr Result(SingleArg &&arg, Args &&...args)
        : m_value(std::forward<SingleArg>(arg), std::forward<Args>(args)...),
          m_error(Error::Success)
    {
    }

    /// Returns a copy of the stored value, or a copy of the defaultValue
    /// argument if this stores an error.
    constexpr T value_or(const T &defaultValue) NOEXCEPT
    {
        return isError() ? defaultValue : this->m_value;
    }

    template <typename Callable>
        requires std::is_invocable_v<Callable, const T &>
    constexpr auto map(Callable &&callable) const &NOEXCEPT
    {
        using TransformedType = std::invoke_result_t<Callable, const T &>;
        using ReturnType = Result<TransformedType, Error>;

        if (isError()) {
            return ReturnType(this->error());
        } else {
            return ReturnType(
                std::invoke(std::forward<Callable>(callable), this->m_value));
        }
    }

    template <typename Callable>
        requires std::is_invocable_v<Callable, T &>
    constexpr auto map(Callable &&callable) & NOEXCEPT
    {
        using TransformedType = std::invoke_result_t<Callable, T &>;
        using ReturnType = Result<TransformedType, Error>;

        if (isError()) {
            return ReturnType(this->error());
        } else {
            return ReturnType(
                std::invoke(std::forward<Callable>(callable), this->m_value));
        }
    }

    template <typename Callable>
        requires std::is_invocable_v<Callable, T &>
    constexpr auto map(Callable &&callable) && NOEXCEPT
    {
        using TransformedType = std::invoke_result_t<Callable, T &&>;
        using ReturnType = Result<TransformedType, Error>;

        if (isError()) {
            return ReturnType(this->error());
        } else {
            return ReturnType(std::invoke(std::forward<Callable>(callable),
                                          std::move(this->m_value)));
        }
    }
};

// version of Result that stores a Reference
template <typename T, ErrorLike Error>
    requires(std::is_lvalue_reference_v<T> and
             !std::is_constructible_v<T, Error> and
             !std::is_constructible_v<Error, T>)
struct Result<T, Error>
{
  private:
    std::remove_reference_t<T> *m_value;
    Error m_error;

  public:
    [[nodiscard]] T value() const &
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return *m_value;
    }

    [[nodiscard]] T value_or(T defaultValue) const &
    {
        return isError() ? defaultValue : value();
    }

    [[nodiscard]] T operator->() const &
    {
        uassert(isSuccess(),
                "Attempt to get value of a result that is not a success");
        return *m_value;
    }

    [[nodiscard]] Error error() const { return m_error; }

    [[nodiscard]] constexpr bool isError() const
    {
        return m_error != Error::Success;
    }
    [[nodiscard]] constexpr bool isSuccess() const
    {
        return m_error == Error::Success;
    }

    constexpr explicit operator bool() const { return isSuccess(); }

    Result() = delete;

    constexpr Result(Error error) : m_value{}, m_error(error)
    {
        uassert(error != Error::Success,
                "Attempt to construct error with a success value, probably not "
                "what you wanted");
    }

    constexpr Result(T arg)
        : m_value(std::addressof(arg)), m_error(Error::Success)
    {
    }

    template <typename Callable>
        requires std::is_invocable_v<Callable, T>
    constexpr auto map(Callable &&callable) const NOEXCEPT
    {
        using TransformedType = std::invoke_result_t<Callable, T>;
        using ReturnType = Result<TransformedType, Error>;

        if (isError()) {
            return ReturnType(this->error());
        } else {
            return ReturnType(
                std::invoke(std::forward<Callable>(callable), this->value()));
        }
    }
};

template <ErrorLike T> [[nodiscard]] constexpr bool isSuccess(T error)
{
    return error == T::Success;
}

template <typename T, ErrorLike E>
[[nodiscard]] constexpr bool isSuccess(const Result<T, E> &result)
{
    return result.isSuccess();
}

#endif
