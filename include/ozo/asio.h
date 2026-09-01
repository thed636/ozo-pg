#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/is_executor.hpp>
#include <boost/asio/execution/executor.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/execution/context.hpp>

namespace ozo {

namespace asio = boost::asio;

/**
 * @brief A Boost.Asio executor
 *
 * Accepts both the current executor model and the older one, since Boost
 * still ships types satisfying only the latter.
 */
template <typename T>
concept AsioExecutor = asio::execution::is_executor<std::decay_t<T>>::value
    || asio::is_executor<std::decay_t<T>>::value;
using asio::async_completion;
using asio::io_context;

using asio::async_initiate;

namespace detail {

template <typename Executor>
struct strand_executor {
    using type = asio::strand<Executor>;

    static auto get(const Executor& ex = Executor{}) {
        return type{ex};
    }
};

template <typename Executor>
using strand = typename strand_executor<std::decay_t<Executor>>::type;

template <typename Executor>
auto make_strand_executor(const Executor& ex) {
    return strand_executor<Executor>::get(ex);
}

/**
 * @brief Timer type and factory for an executor
 *
 * Works for any Boost.Asio executor. The timer keeps the executor's own type
 * rather than erasing it to `asio::any_io_executor`, so a timer created for a
 * strand stays on that strand and costs no type erasure.
 *
 * This remains a customization point: specialize it to supply a different timer
 * for a particular executor, as the tests do to drive time from a mock.
 */
template <typename Executor>
struct operation_timer {
    static_assert(AsioExecutor<Executor>,
        "operation_timer<> requires a Boost.Asio executor; specialize it for other types");

    using type = asio::basic_waitable_timer<
        std::chrono::steady_clock,
        asio::wait_traits<std::chrono::steady_clock>,
        Executor>;

    template <typename TimeConstraint>
    static type get(const Executor& ex, TimeConstraint t) {
        return type{ex, t};
    }

    static type get(const Executor& ex) {
        return type{ex};
    }
};

template <typename Executior, typename TimeConstraint>
inline auto get_operation_timer(const Executior& ex, TimeConstraint t) {
    return operation_timer<Executior>::get(ex, t);
}

template <typename Executor>
inline auto get_operation_timer(const Executor& ex) {
    return operation_timer<Executor>::get(ex);
}


/**
 * @brief Descriptor type and factory for an executor
 *
 * Works for any Boost.Asio executor, and like `operation_timer` keeps the
 * executor's own type. Note that the descriptor is built from the executor
 * directly rather than from `ex.context()`: reaching for the execution context
 * is what previously restricted this to `io_context::executor_type`, since no
 * other executor is required to expose one.
 *
 * This remains a customization point, as the tests rely on.
 */
template <typename Executor>
struct connection_stream {
    static_assert(AsioExecutor<Executor>,
        "connection_stream<> requires a Boost.Asio executor; specialize it for other types");

    using type = asio::posix::basic_stream_descriptor<Executor>;

    static type get(const Executor& ex, typename type::native_handle_type fd) {
        return type{ex, fd};
    }

    static type get(const Executor& ex) {
        return type{ex};
    }
};

template <typename Executior, typename NativeHandle>
inline auto get_connection_stream(const Executior& ex, NativeHandle fd) {
    return connection_stream<Executior>::get(ex, fd);
}

template <typename Executor>
inline auto get_connection_stream(const Executor& ex) {
    return connection_stream<Executor>::get(ex);
}

} // namespace detail

template <typename Operation>
struct get_operation_initiator_impl {
    constexpr static auto apply(const Operation& op) {
        return op.get_initiator();
    }
};

/**
 * @brief Get asynchronous operation initiator
 *
 * Initiator is a functional object which may be used with
 * [boost::asio::async_initiate](https://www.boost.org/doc/libs/1_70_0/doc/html/boost_asio/reference/async_initiate.html)
 * to start an asynchronous operation. Typically this is detail zone of an operation,
 * but in OZO this is a part of operations customization for extentions like failover.
 *
 * @param op --- asynchronous operation object
 * @return initiator functional object
 *
 * ###Customization Point
 *
 * The function is implemented via `ozo::get_operation_initiator_impl`. The default
 * implementation is:
@code
template <typename Operation>
struct get_operation_initiator_impl {
    constexpr static auto apply(const Operation& op) {
        return op.get_initiator();
    }
};
@endcode
 *
 * So this behaviour may be changed via specialization of the template.
 * @ingroup group-core-functions
 */
template <typename Operation>
constexpr auto get_operation_initiator(const Operation& op) {
    return get_operation_initiator_impl<Operation>::apply(op);
}

template <typename Factory, typename Operation>
struct construct_initiator_impl {
    constexpr static auto apply(const Factory&, const Operation&) {
        static_assert(std::is_void_v<Factory>, "Factory is not supported for the Operation");
    }
};

/**
 * @brief Create asynchronous operation initiator using factory
 *
 * The function constructs asynchronous operation initiator using factory object.
 * The default behaviour is static assertion. The behaviour should be customized
 * for a particular factory.
 *
 * @param f --- factory for asynchronous operation initiator object.
 * @param op --- asynchronous operation object.
 * @return initiator functional object.
 *
 * ###Customization Point
 *
 * Default implementation is static assertion
 * @code
template <typename Factory, typename Operation>
struct construct_initiator_impl {
    constexpr static auto apply(const Factory&, const Operation&) {
        static_assert(std::is_void_v<Factory>, "Factory is not supported for the Operation");
    }
};
 * @endcode
 *
 * @ingroup group-core-functions
 */
template <typename Operation, typename Factory>
constexpr auto construct_initiator(Factory&& f, const Operation& op) {
    return construct_initiator_impl<std::decay_t<Factory>, Operation>::apply(f, op);
}

/**
 * @brief Base class for async operations
 *
 * Base class for async operation which provides facilities for initiator rebinding.
 * Should be used by all the operations which support extentions like failover via
 * initiator rebinding.
 *
 * ### Example
 *
 * Execute operation may look like this (simplified, for exposition only):
 *
 * @code
template <typename Initiator>
struct execute_op : base_async_operation <execute_op, Initiator> {
    using base = typename execute_op::base;
    using base::base;

    template <typename P, typename Q, typename TimeConstraint, typename CompletionToken>
    decltype(auto) operator() (P&& provider, Q&& query, TimeConstraint t, CompletionToken&& token) const {
        return async_initiate<CompletionToken, handler_signature<P>>(
            get_operation_initiator(*this), token, std::forward<P>(provider), t, std::forward<Q>(query));
    }
};

constexpr execute_op<impl::initiate_async_execute> execute;
 * @endcode
 * @ingroup group-core-types
 */
template <typename Operation, typename Initiator>
struct base_async_operation {
    using initiator_type = Initiator;
    using base = base_async_operation;
    initiator_type initiator_;

    constexpr base_async_operation(const Initiator& initiator = Initiator{}) : initiator_(initiator) {}

    constexpr initiator_type get_initiator() const { return initiator_;}

    template <typename InitiatorFactory>
    constexpr auto operator[] (InitiatorFactory&& f) const {
        const auto& op = static_cast<const Operation&>(*this);
        return op.rebind_initiator(construct_initiator(std::forward<InitiatorFactory>(f), *this));
    }
};

} // namespace ozo
