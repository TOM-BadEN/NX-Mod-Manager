/**
 * AsyncFurture - 异步任务封装
 * 来源项目：https://github.com/ITotalJustice/untitled
 */

#pragma once

#include <future>
#include <stop_token>

namespace util {

// simple wrapper for future + stop token.
// todo: write my own std::async with token source added, like jthread.
template<typename T>
class AsyncFurture {
public:
    constexpr AsyncFurture() = default;
    constexpr AsyncFurture(AsyncFurture&& token)
    : future{std::move(token.future)}
    , stop_source{std::move(token.stop_source)} {}
    constexpr AsyncFurture(std::future<T>&& f)
    : future{std::forward<std::future<T>>(f)} {}
    constexpr AsyncFurture(std::future<T>&& f, std::stop_source&& ss)
    : future{std::forward<std::future<T>>(f)}
    , stop_source{std::forward<std::stop_source>(ss)} {}
    ~AsyncFurture() {
        if (this->future.valid()) {
            this->stop_source.request_stop();
            this->future.get();
        }
    }

    // disable copying
    AsyncFurture(const AsyncFurture&) = delete;
    AsyncFurture& operator=(const AsyncFurture& f) = delete;

    AsyncFurture<T>& operator=(AsyncFurture<T>&& f) noexcept {
        this->future = std::move(f.future);
        this->stop_source = std::move(f.stop_source);
        return *this;
    }

    [[nodiscard]]
    auto get() {
        return this->future.get();
    }

    [[nodiscard]]
    auto get_token() const noexcept {
        return this->stop_source.get_token();
    }

    auto request_stop() {
        return this->stop_source.request_stop();
    }

    [[nodiscard]]
    auto stop_possible() {
        return this->stop_source.stop_possible();
    }

    [[nodiscard]]
    auto wait() {
        return this->future.wait();
    }

    template<typename Rep, typename Period>
    [[nodiscard]]
    auto wait_for(const std::chrono::duration<Rep, Period>& wait_time) const {
        return this->future.wait_for(wait_time);
    }

    template<typename Clock, typename Duration>
    [[nodiscard]]
    auto wait_until(const std::chrono::time_point<Clock, Duration>& wait_time) {
        return this->future.wait_until(wait_time);
    }

    [[nodiscard]]
    auto valid() const noexcept {
        return this->future.valid();
    }

private:
    std::future<T> future{};
    std::stop_source stop_source{};
};

template<typename Fn, typename... Args>
using AsyncResult = typename std::invoke_result<
    typename std::decay<Fn>::type, typename std::decay<Args>::type...>::type;

// enabled if function DOES start with std::stop_token
template<typename Fn, typename... Args, typename = std::enable_if<std::is_invocable_v<std::decay_t<Fn>, std::stop_token, std::decay_t<Args>...>>>
auto async(Fn&& fn, Args&&... args) -> AsyncFurture<AsyncResult<Fn, std::stop_token, Args...>> {
    std::stop_source source_token;
    return AsyncFurture{
        std::async(std::launch::async, std::forward<Fn>(fn), source_token.get_token(), std::forward<Args>(args)...),
        std::move(source_token)
    };
}

// enabled if function does NOT start with std::stop_token
template<typename Fn, typename... Args, typename = std::enable_if<!std::is_invocable_v<std::decay_t<Fn>, std::stop_token, std::decay_t<Args>...>>>
auto async(Fn&& fn, Args&&... args) -> AsyncFurture<AsyncResult<Fn, Args...>> {
    return AsyncFurture{
        std::async(std::launch::async, std::forward<Fn>(fn), std::forward<Args>(args)...)
    };
}

} // namespace util
