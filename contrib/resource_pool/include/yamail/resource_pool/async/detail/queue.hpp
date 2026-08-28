#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>

#include <boost/asio/execution.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/query.hpp>

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>

namespace yamail {
namespace resource_pool {
namespace async {

namespace asio = boost::asio;

namespace detail {

using clock = std::chrono::steady_clock;

template <class Handler>
class expired_handler {
    Handler handler;

public:
    using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

    expired_handler() = default;

    template <class HandlerT>
    explicit expired_handler(HandlerT&& handler,
            std::enable_if_t<!std::is_same_v<std::decay_t<HandlerT>, expired_handler>, void*> = nullptr)
            : handler(std::forward<HandlerT>(handler)) {
        static_assert(std::is_same_v<std::decay_t<HandlerT>, Handler>, "HandlerT is not Handler");
    }

    void operator ()() {
        handler(make_error_code(error::get_resource_timeout));
    }

    void operator ()() const {
        handler(make_error_code(error::get_resource_timeout));
    }

    auto get_executor() const noexcept {
        return asio::get_associated_executor(handler);
    }
};

template <class Handler>
expired_handler(Handler&&) -> expired_handler<std::decay_t<Handler>>;

template <class Value, class Executor>
struct queued_value {
    Value request;
    Executor executor;
};

template <class Value, class Mutex, class Executor, class Timer>
class queue : public std::enable_shared_from_this<queue<Value, Mutex, Executor, Timer>> {
public:
    using value_type = Value;
    using executor_type = Executor;
    using timer_t = Timer;
    using queued_value_t = queued_value<value_type, executor_type>;

    queue(std::size_t capacity) : _capacity(capacity) {}

    queue(const queue&) = delete;

    queue(queue&&) = delete;

    std::size_t capacity() const noexcept { return _capacity; }
    std::size_t size() const noexcept;
    bool empty() const noexcept;
    const timer_t& timer(const executor_type& executor);

    bool push(const executor_type& executor, time_traits::duration wait_duration, value_type&& request);
    boost::optional<queued_value_t> pop();

private:
    using mutex_t = Mutex;
    using lock_guard = std::lock_guard<mutex_t>;

    struct expiring_request {
        using list = std::list<expiring_request>;
        using list_it = typename list::iterator;
        using multimap = std::multimap<time_traits::time_point, expiring_request*>;
        using multimap_it = typename multimap::iterator;

        executor_type executor;
        queue::value_type request;
        list_it order_it;
        multimap_it expires_at_it;

        expiring_request() = default;
    };

    using request_multimap_value = typename expiring_request::multimap::value_type;
    using timers_map = typename std::unordered_map<const asio::execution_context*, timer_t>;

    const std::size_t _capacity;
    mutable mutex_t _mutex;
    typename expiring_request::list _ordered_requests_pool;
    typename expiring_request::list _ordered_requests;
    typename expiring_request::multimap _expires_at_requests;
    timers_map _timers;

    bool fit_capacity() const { return _expires_at_requests.size() < _capacity; }
    void cancel(boost::system::error_code ec, time_traits::time_point expires_at);
    void update_timer();
    timer_t& get_timer(const executor_type& executor);

    static const asio::execution_context* context_of(const executor_type& executor) {
        return std::addressof(asio::query(executor, asio::execution::context));
    }
};

template <class V, class M, class E, class T>
std::size_t queue<V, M, E, T>::size() const noexcept {
    const lock_guard lock(_mutex);
    return _expires_at_requests.size();
}

template <class V, class M, class E, class T>
bool queue<V, M, E, T>::empty() const noexcept {
    const lock_guard lock(_mutex);
    return _ordered_requests.empty();
}

template <class V, class M, class E, class T>
const typename queue<V, M, E, T>::timer_t& queue<V, M, E, T>::timer(const executor_type& executor) {
    const lock_guard lock(_mutex);
    return get_timer(executor);
}

template <class V, class M, class E, class T>
bool queue<V, M, E, T>::push(const executor_type& executor, time_traits::duration wait_duration, value_type&& request) {
    const lock_guard lock(_mutex);
    if (!fit_capacity()) {
        return false;
    }
    if (_ordered_requests_pool.empty()) {
        _ordered_requests_pool.emplace_back();
    }
    const auto order_it = _ordered_requests_pool.begin();
    _ordered_requests.splice(_ordered_requests.end(), _ordered_requests_pool, order_it);
    expiring_request& req = *order_it;
    req.executor = executor;
    req.request = std::move(request);
    req.order_it = order_it;
    const auto expires_at = time_traits::add(time_traits::now(), wait_duration);
    req.expires_at_it = _expires_at_requests.insert(std::make_pair(expires_at, &req));
    update_timer();
    return true;
}

template <class V, class M, class E, class T>
boost::optional<typename queue<V, M, E, T>::queued_value_t> queue<V, M, E, T>::pop() {
    const lock_guard lock(_mutex);
    if (_ordered_requests.empty()) {
        return {};
    }
    const auto ordered_it = _ordered_requests.begin();
    expiring_request& req = *ordered_it;
    queued_value_t result {std::move(req.request), req.executor};
    _expires_at_requests.erase(req.expires_at_it);
    _ordered_requests_pool.splice(_ordered_requests_pool.begin(), _ordered_requests, ordered_it);
    update_timer();
    return { std::move(result) };
}

template <class V, class M, class E, class T>
void queue<V, M, E, T>::cancel(boost::system::error_code ec, time_traits::time_point expires_at) {
    if (ec) {
        return;
    }
    const lock_guard lock(_mutex);
    const auto begin = _expires_at_requests.begin();
    const auto end = _expires_at_requests.upper_bound(expires_at);
    std::for_each(begin, end, [&] (request_multimap_value& v) {
        const auto req = v.second;
        asio::post(req->executor, expired_handler(std::move(req->request)));
        _ordered_requests_pool.splice(_ordered_requests_pool.begin(), _ordered_requests, req->order_it);
    });
    _expires_at_requests.erase(_expires_at_requests.begin(), end);
    update_timer();
}

template <class V, class M, class E, class T>
void queue<V, M, E, T>::update_timer() {
    using timers_map_value = typename timers_map::value_type;
    if (_expires_at_requests.empty()) {
        std::for_each(_timers.begin(), _timers.end(), [] (timers_map_value& v) { v.second.cancel(); });
        _timers.clear();
        return;
    }
    const auto earliest_expire = _expires_at_requests.begin();
    const auto expires_at = earliest_expire->first;
    auto& timer = get_timer(earliest_expire->second->executor);
    timer.expires_at(expires_at);
    std::weak_ptr<queue> weak(this->shared_from_this());
    timer.async_wait([weak, expires_at] (boost::system::error_code ec) {
        if (const auto locked = weak.lock()) {
            locked->cancel(ec, expires_at);
        }
    });
}

template <class V, class M, class E, class T>
typename queue<V, M, E, T>::timer_t& queue<V, M, E, T>::get_timer(const executor_type& executor) {
    const auto* context = context_of(executor);
    auto it = _timers.find(context);
    if (it != _timers.end()) {
        return it->second;
    }
    return _timers.emplace(context, timer_t(executor)).first->second;
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
