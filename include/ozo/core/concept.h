#pragma once

#ifndef BOOST_HANA_CONFIG_ENABLE_STRING_UDL
#error "OZO needs BOOST_HANA_CONFIG_ENABLE_STRING_UDL to be defined"
#endif

#include <boost/fusion/adapted.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/fusion/include/is_sequence.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/string.hpp>
#include <typeinfo>
#include <type_traits>
#include <iterator>

namespace ozo {

/**
 * @defgroup group-core-concepts Concepts
 * @ingroup group-core
 * @brief Library-wide concepts
 *
 * The library is built around concepts, so that it is easy to extend, adapt and
 * test. They are C++20 concepts, constraining templates directly:
 *
 * @code
    template <typename T>
    decltype(auto) unwrap(T&& c) requires (!Nullable<T>) {
        return c;
    }

    template <Nullable T>
    decltype(auto) unwrap(T&& c) {
        return *c;
    }
 * @endcode
 */
///@{


/**
 * @brief Negation operator support concept
 *
 * Type T models `%OperatorNot` concept if for its object `t` valid `static_assert(std::is_same_v<decltyp(!t), bool>,"")`.
 *
 * @concept{OperatorNot}
 */
//! @cond
template <typename T>
concept OperatorNot = requires (std::decay_t<T> t) { !t; };
//! @endcond


/**
 * @brief Output Iterator concept
 *
 * Type Iterator models `%OutputIterator` concept if for its iterator category is based on `std::output_iterator_tag`.
 *
 * @concept{OutputIterator}
 */
//! @cond
template <typename T>
concept OutputIterator = requires {
    requires std::is_base_of_v<
        std::output_iterator_tag,
        typename std::iterator_traits<T>::iterator_category>;
};
//! @endcond


/**
 * @brief Forward Iterator concept
 *
 * Type `Iterator` models `%ForwardIterator` concept if for its iterator category is based on `std::forward_iterator_tag`.
 *
 * @concept{ForwardIterator}
 */
//! @cond
template <typename T>
concept ForwardIterator = requires {
    requires std::is_base_of_v<
        std::forward_iterator_tag,
        typename std::iterator_traits<T>::iterator_category>;
};
//! @endcond


/**
 * @brief Iterable concept
 *
 * Type `T` models `%Iterable` concept if for its object `t` these requirements are valid.
 *
 * | Expression | Type | Description |
 * |------------|------|-------------|
 * | <PRE>begin(t)</PRE> | ForwardIterator | Should return iterator object that models ForwardIterator concept. |
 * | <PRE>end(t)</PRE> | ForwardIterator | Should return iterator object that models ForwardIterator concept. |
 *
 * @concept{Iterable}
 */
//! @cond
template <typename T>
concept Iterable = requires (T v) {
    begin(v);
    end(v);
} && ForwardIterator<decltype(begin(std::declval<T>()))>
  && ForwardIterator<decltype(end(std::declval<T>()))>;
//! @endcond


/**
 * @brief Insert Iterator concept
 *
 * This trait determines whether T is an insert iterator bound with some container
 * Type `Iterator` models `%InsertIterator` concept if it models `OutputIterator` or
 * has `container_type` member type which is class.
 *
 * @concept{InsertIterator}
 */
//! @cond
template <typename T>
concept InsertIterator = OutputIterator<T> && requires {
    requires std::is_class_v<typename T::container_type>;
};
//! @endcode

/**
 * @brief Boost.Fusion Sequence concept
 *
 * Type `T` models `%FusionSequence` concept if `boost::fusion::traits::is_sequence<std::decay_t<T>>::value` is `true`.
 * @concept{FusionSequence}
 */
//! @cond
template <typename T>
concept FusionSequence = boost::fusion::traits::is_sequence<std::decay_t<T>>::value;
//! @endcode

/**
 * @brief Boost.Hana Sequence concept
 *
 * Shortcut for [boost::hana::Sequence](https://www.boost.org/doc/libs/1_67_0/libs/hana/doc/html/group__group-Sequence.html) concept.
 * @concept{HanaSequence}
 */
//! @cond
template <typename T>
concept HanaSequence = boost::hana::Sequence<std::decay_t<T>>::value;
//! @endcode

/**
 * @brief Boost.Hana Structure concept
 *
 * Shortcut for [boost::hana::Struct](https://www.boost.org/doc/libs/1_67_0/libs/hana/doc/html/group__group-Struct.html) concept.
 * @concept{HanaStruct}
 */
//! @cond
template <typename T>
concept HanaStruct = boost::hana::Struct<std::decay_t<T>>::value;
//! @endcode

/**
 * @brief Boost.Hana String concept
 *
 * `HanaString` the only concrete model is [boost::hana::string](https://www.boost.org/doc/libs/1_67_0/libs/hana/doc/html/structboost_1_1hana_1_1string.html).
 * @concept{HanaString}
 */
//! @cond
template <typename T>
concept HanaString = decltype(boost::hana::is_a<boost::hana::string_tag>(std::declval<T>()))::value;
//! @endcode

/**
 * @brief Boost.Hana Tuple concept
 *
 * `HanaTuple` the only concrete model is [boost::hana::tuple](https://www.boost.org/doc/libs/1_67_0/libs/hana/doc/html/structboost_1_1hana_1_1tuple.html).
 * @concept{HanaTuple}
 */
//! @cond
template <typename T>
concept HanaTuple = decltype(boost::hana::is_a<boost::hana::tuple_tag>(std::declval<T>()))::value;
//! @endcode




/**
 * @brief Boost.Fusion Adapted Structure concept
 *
 * Type `T` models `%FusionAdaptedStruct` if `T` is a structure adapted via
 * the [Boost.Fusion](https://www.boost.org/doc/libs/1_67_0/libs/fusion/doc/html/fusion/adapted.html)
 * adaptation mechanisms.
 * @concept{FusionAdaptedStruct}
 */
//! @cond
template <typename T>
concept FusionAdaptedStruct = requires {
    // Boost.Fusion tags every adapted type; a struct adapted through the
    // adaptation macros is tagged struct_tag.
    requires std::is_same_v<
        typename boost::fusion::traits::tag_of<std::decay_t<T>>::type,
        boost::fusion::struct_tag>;
};
//! @endcode

/**
 * @brief Integral concept
 *
 * Integral type concept, shortcut to `std::is_integral_v`.
 * @concept{Integral}
 */
//! @cond
template <typename T>
concept Integral = std::is_integral_v<std::decay_t<T>>;
//! @endcode

/**
 * @brief Floating Point concept
 *
 * Floating point type concept, shortcut to `std::is_floating_point_v`.
 * @concept{FloatingPoint}
 */
//! @cond
template <typename T>
concept FloatingPoint = std::is_floating_point_v<std::decay_t<T>>;
//! @endcode

namespace detail {

// Detection and the answer it produces, kept together: the type has to offer
// std::data and std::size, and the pointer std::data yields must be writable
// unless the container itself is const.
template <typename T>
concept has_std_size_data = requires (T& v) { std::data(v) + std::size(v); };

template <typename T>
struct std_size_data_compatible {
    static constexpr bool value = false;
};

template <has_std_size_data T>
struct std_size_data_compatible<T> {
    static constexpr bool value = !std::is_const_v<std::remove_pointer_t<decltype(std::data(std::declval<T&>()))>> || std::is_const_v<T>;
};

// The same, for data() and size() found by argument-dependent lookup.
template <typename T>
concept has_adl_size_data = requires (T& v) { data(v) + size(v); };

template <typename T>
struct adl_size_data_compatible {
    static constexpr bool value = false;
};

template <has_adl_size_data T>
struct adl_size_data_compatible<T> {
    static constexpr bool value = !std::is_const_v<std::remove_pointer_t<decltype(data(std::declval<T&>()))>> || std::is_const_v<T>;
};

template <typename T>
constexpr auto sizeof_value_type(T& v) {
    if constexpr (std_size_data_compatible<T>::value) {
        return std::integral_constant<std::size_t, sizeof(decltype(*std::data(v)))>{};
    } else if constexpr (adl_size_data_compatible<T>::value) {
        return std::integral_constant<std::size_t, sizeof(decltype(*data(v)))>{};
    } else {
        return std::integral_constant<std::size_t, 0>{};
    }
}

template <typename T>
constexpr std::size_t sizeof_value_type() {
    return decltype(sizeof_value_type(std::declval<T&>()))::value;
}

} // namespace detail


/**
 * @brief `RawDataWritable` concept
 *
 * Indicates if `T` can be written as a sequence of bytes without endian conversion.
 * `RawDataWritable<T>` is `true` if for object `v` of type `T` applicable one of this code:
 * @code
    auto raw = std::data(v);          // std_size_data_compatible<T,
    *raw = 1;                         //
    static_assert(sizeof(*raw) == 1); //                  1>
    auto n = std::size(v);            // support_std_size<T>
 * @endcode
 * or
 * @code
    auto raw = data(v);               // adl_size_data_compatible<T,
    *raw = 1;                         //
    static_assert(sizeof(*raw) == 1); //                         1>
    auto n = size(v);                 // support_external_size<T>
 * @endcode
 * @tparam T - type to examine
 * @hideinitializer
 */
template <typename T>
concept RawDataWritable =
    !std::is_const_v<std::remove_reference_t<T>>
    && detail::sizeof_value_type<std::remove_reference_t<T>>() == 1;

/**
 * @brief `RawDataReadable` concept
 *
 * Indicates if `T` can be read as a sequence of bytes without endian conversion.
 * `RawDataReadable<T>` is `true` if for object `v` of type `T` applicable one of this code:
 * @code
    auto raw = std::data(std::as_const(v)); // std_size_data_compatible<const T,
    auto v = *raw;                          //
    static_assert(sizeof(v) == 1);          //                1>
    auto n = std::size(v);                  // support_std_size<T>
 * @endcode
 * or
 * @code
    auto raw = data(std::as_const(v));   // adl_size_data_compatible<const T,
    auto v = *raw;                       //
    static_assert(sizeof(v) == 1);       //                       1>
    auto n = size(v);                    // support_external_size<T>
 * @endcode
 * @tparam T - type to examine
 * @hideinitializer
 */
template <typename T>
concept RawDataReadable = detail::sizeof_value_type<std::add_const_t<std::remove_reference_t<T>>>() == 1;

/**
 * @brief Emplaceable concept
 *
 * Indicates if container T can emplace it's element with default constructor
 * @tparam T - type to examine
 * @hideinitializer
 */
template <typename T>
concept Emplaceable = requires (std::decay_t<T>& v) { v.emplace(); };

template <typename T>
struct is_time_constraint : std::false_type {};

/**
 * @brief Time constraint concept
 *
 * `%TimeConstraint` is a type which provides information about time restrictions for an operation.
 *
 * @par Concrete models
 *
 * * `std::chrono::duration` --- operation time-out duration,
 * * `std::chrono::time_point` --- operation deadline time point,
 * * `ozo::none` --- operation is not restricted in time.
 *
 * @concept{TimeConstraint}
 */
//! @cond
template <typename T>
concept TimeConstraint = is_time_constraint<std::decay_t<T>>::value;
//! @endcond

/**
 * @brief Completion token concept
 *
 * `CompletionToken` is an entity which determines how to continue with asynchronous operation result when
 * the operation is complete. According to <a href="https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/async_completion.html">
 * `boost::asio::async_completion`</a> it defines the return value of an asynchronous function.
 *
 * Assume we have an asynchronous IO function:
 * @code
template <typename ConnectionProvider, typename CompletionToken>
auto async_io(ConnectionProvider&&, Param1 p1, ..., CompletionToken&&);
 * @endcode
 *
 * Then the result type of the function depends on `CompletionToken`, and `CompletionToken` - is any of these next entities:
 * * #Handler concept implementation. Asynchronous function in this case will return `void`.
 * In this case the equivalent function signature will be:
 * @code
template <typename ConnectionProvider>
void async_io(ConnectionProvider, Param1 p1, ..., Handler);
 * @endcode
 *
 * * <a href= "https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/use_future.html">
 * `boost::asio::use_future`</a> - to get a future on the asynchronous operation result.
 * Asynchronous function in this case will return `std::future<Connection>`.
 * In this case the equivalent function signature will be:
 * @code
template <typename ConnectionProvider>
std::future<ozo::connection_type<ConnectionProvider>> async_io(
    ConnectionProvider&&, Param1 p1, ..., boost::asio::use_future_t);
 * @endcode
 *
 * * <a href="https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/basic_yield_context.html">
 * `boost::asio::yield_context`</a> - to use async operation with Boost.Coroutine.
 * Asynchronous function in this case will return `Connection`.
 * In this case the equivalent function signature will be:
 * @code
template <typename ConnectionProvider>
ozo::connection_type<ConnectionProvider> async_io(
    ConnectionProvider&&, Param1 p1, ..., boost::asio::yield_context);
 * @endcode
 *
 * * any other type supported with <a href="https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/async_completion.html">
 * `boost::asio::async_completion`</a> mechanism.
 * Asynchronous function in this case will return a type is depends on
 * <a href="https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/async_completion/result.html">
 * `boost::asio::async_completion::result`</a>.
 * @concept{CompletionToken}
 */

/**
 * @brief Handler concept
 *
 * `Handler` is a function or a functor which is used as a callback for handling result of asynchronous IO operations in the library.
 *
 * In case of function it has to have this signature:
 *@code
template <typename Connection>
void Handler(ozo::error_code ec, Connection&& connection) {
    //...
}
 *@endcode
 *
 * In case of functor it has to have such `operator()`:
 *@code
struct Handler {
    template <typename Connection>
    void operator() (ozo::error_code ec, Connection&& connection) {
        //...
    }
};
 *@endcode
 *
 * In case of lambda:
 *@code
auto Handler = [&] (ozo::error_code ec, auto connection) {
    //...
};
 *@endcode
 *
 * `Handler` has to handle an `ozo::error_code` object as first argument, and the `Connection` implementation
 * object as a second one. It is better to define second argument as a template parameter because the
 * implementation depends on a numerous of compile-time options but if it is really needed - real type
 * can be obtained with `ozo::connection_type`.
 *
 * `Handler` has to be invoked according to `ec` state:
 * * **false** --- operation succeeded, `Connection` should be in good state and can be used for an operation.
 * * **true** --- operation failed and `ec` contains error, `Connection` could be in these states:
 *   * `Connection` is in **null-state** --- `ozo::is_null_recursive()` returns `true`, object is useless;
 *   * `Connection` is in **bad state** --- `ozo::is_null_recursive()` returns `false`,
 *                   `ozo::connection_bad()` returns true or
 *                   `ozo::get_transaction_status()` returns not `ozo::transaction_status::idle`,
 *                    object may not be used for further operations but it may provide additional
 *                    error context via `ozo::error_message()` and `ozo::get_error_context()` functions.
 *   * `Connection` is in **good state** --- `ozo::is_null_recursive()` returns `false`,
 *                   `ozo::connection_bad()` returns true and
 *                   `ozo::get_transaction_status()` returns `ozo::transaction_status::idle`,
 *                    object may be used for further operations and may provide additional error context via
 *                   `ozo::error_message()` and `ozo::get_error_context()` functions.
 * @concept{Handler}
 */
///@}

} // namespace ozo
