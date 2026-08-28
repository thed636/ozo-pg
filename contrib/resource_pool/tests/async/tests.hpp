#ifndef YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
#define YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP

#include <yamail/resource_pool/time_traits.hpp>
#include <yamail/resource_pool/detail/idle.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/execution_context.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <functional>
#include <list>
#include <memory>

namespace tests {

namespace asio = boost::asio;

using namespace testing;
using namespace yamail::resource_pool;

struct resource {
    resource() = default;
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource&) = delete;
    resource& operator =(resource&&) = default;
};

struct request {
    int value;
};

// Boost.Asio no longer has separate post, dispatch and defer entry points on an
// executor: all three go through execute(), and what used to distinguish them is
// now expressed as blocking and relationship properties applied beforehand.
//
// The mock therefore records execute() alone. The tests that used to assert
// which of the three was used were really asserting that work reached the
// executor at all -- each captures the submitted function and invokes it by
// hand -- and the old post-versus-dispatch choice is not one the current
// library makes in the same places, so asserting it would be asserting an
// implementation detail of a Boost version rather than the pool's behaviour.
struct executor_mock {
    virtual ~executor_mock() = default;
    virtual void execute(std::function<void ()>) const = 0;
};

struct executor_gmock : executor_mock {
    MOCK_CONST_METHOD1(execute, void (std::function<void ()>));
};

template <class Handler>
struct shared_wrapper {
    std::shared_ptr<Handler> ptr;

    template <class ... Args>
    void operator ()(Args&& ... args) {
        return (*ptr)(std::forward<Args>(args) ...);
    }
};

template <class Function>
auto wrap_shared(Function&& f) {
    return shared_wrapper<std::decay_t<Function>> {
        std::make_shared<std::decay_t<Function>>(std::forward<Function>(f))};
}

// A conforming Boost.Asio executor that records what was submitted to it.
//
// It has to satisfy asio::any_io_executor, because the pool type-erases the
// executor associated with a handler into one. That requires execute(),
// equality, a context query, and support for the blocking, relationship and
// outstanding_work properties -- the last three only need to be answerable, so
// they are reported statically and the requires and prefers are identities.
struct mocked_executor {
    const executor_mock* impl = nullptr;
    asio::execution_context* context_ = nullptr;

    asio::execution_context& context() const noexcept {
        return *context_;
    }

    template <class Function>
    void execute(Function&& f) const {
        impl->execute(wrap_shared(std::forward<Function>(f)));
    }

    asio::execution_context& query(asio::execution::context_t) const noexcept {
        return *context_;
    }

    // any_io_executor requires blocking.never as a static property.
    static constexpr asio::execution::blocking_t::never_t
    query(asio::execution::blocking_t) noexcept {
        return {};
    }

    static constexpr asio::execution::relationship_t::fork_t
    query(asio::execution::relationship_t) noexcept {
        return {};
    }

    static constexpr asio::execution::outstanding_work_t::untracked_t
    query(asio::execution::outstanding_work_t) noexcept {
        return {};
    }

    mocked_executor require(asio::execution::blocking_t::never_t) const { return *this; }
    mocked_executor require(asio::execution::blocking_t::possibly_t) const { return *this; }
    mocked_executor prefer(asio::execution::relationship_t::fork_t) const { return *this; }
    mocked_executor prefer(asio::execution::relationship_t::continuation_t) const { return *this; }

    mocked_executor prefer(asio::execution::outstanding_work_t::tracked_t) const { return *this; }
    mocked_executor prefer(asio::execution::outstanding_work_t::untracked_t) const { return *this; }

    friend bool operator ==(const mocked_executor& lhs, const mocked_executor& rhs) noexcept {
        return lhs.context_ == rhs.context_ && lhs.impl == rhs.impl;
    }

    friend bool operator !=(const mocked_executor& lhs, const mocked_executor& rhs) noexcept {
        return !(lhs == rhs);
    }
};

struct mocked_io_context : asio::execution_context {
    using executor_type = mocked_executor;

    executor_type* executor = nullptr;

    mocked_io_context(executor_type* executor)
        : executor(executor) {
        executor->context_ = this;
    }

    executor_type get_executor() const {
        return *executor;
    }
};

} // namespace tests

#endif // YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
