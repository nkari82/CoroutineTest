#pragma once

class Resumable { // The return object
    struct Promise {
        Resumable get_return_object() {
            using Handle = std::coroutine_handle<Promise>;
            return Resumable{ Handle::from_promise(*this) };
        }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<Promise> h_;
    explicit Resumable(std::coroutine_handle<Promise> h) : h_{ h } {}
public:
    using promise_type = Promise;
    Resumable(Resumable&& r) : h_{ std::exchange(r.h_, {}) } {}
    ~Resumable() { if (h_) { h_.destroy(); } }
    bool resume() {
        if (!h_.done()) { h_.resume(); }
        return !h_.done();
    }
};


template <typename T>
class Generator {
    struct Promise {
        T value_;
        auto get_return_object() -> Generator {
            using Handle = std::coroutine_handle<Promise>;
            return Generator{ Handle::from_promise(*this) };
        }
        auto initial_suspend()
        {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept
        {
            return std::suspend_always{};
        }

        void return_void() {}
        void unhandled_exception() { throw; }
        auto yield_value(T&& value)
        {
            value_ = std::move(value);
            return std::suspend_always{};
        }
        auto yield_value(const T& value)
        {
            value_ = value;
            return std::suspend_always{};
        }
    };
    struct Sentinel {};
    struct Iterator {
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        std::coroutine_handle<Promise> h_; // Data member
        Iterator& operator++() {
            h_.resume();
            return *this;
        }
        void operator++(int)
        {
            (void)operator++();
        }
        T operator*() const
        {
            return h_.promise().value_;
        }
        T* operator->() const
        {
            return std::addressof(operator*());
        }
        bool operator==(Sentinel) const
        {
            return h_.done();
        }
    };

    std::coroutine_handle<Promise> h_;
    explicit Generator(std::coroutine_handle<Promise> h)
        : h_{ h }
    {}
public:
    using promise_type = Promise;
    Generator(Generator&& g)
        : h_(std::exchange(g.h_, {}))
    {}
    ~Generator()
    {
        if (h_)
        {
            h_.destroy();
        }
    }

    auto begin()
    {
        h_.resume();
        return Iterator{ h_ };
    }
    auto end()
    {
        return Sentinel{};
    }
};

template <typename T>
auto seq() -> Generator<T> {
    for (T i = {};; ++i) {
        co_yield i;
    }
}
template <typename T>
auto take_until(Generator<T>& gen, T value) -> Generator<T> {
    for (auto&& v : gen) {
        if (v == value) {
            co_return;
        }
        co_yield v;
    }
}
template <typename T>
auto add(Generator<T>& gen, T adder) -> Generator<T> {
    for (auto&& v : gen) {
        co_yield v + adder;
    }
}

template <typename T>
class [[nodiscard]] Task {
    struct Promise {
        std::variant<std::monostate, T, std::exception_ptr> result_;
        std::coroutine_handle<> continuation_; // A waiting coroutine
        auto get_return_object() noexcept { return Task{ *this }; }
        void return_value(T value) {
            result_.template emplace<1>(std::move(value));
        }
        void unhandled_exception() noexcept {
            result_.template emplace<2>(std::current_exception());
        }
        auto initial_suspend() 
        { 
            return std::suspend_always{}; 
        }

        auto final_suspend() noexcept {
            struct Awaitable {
                bool await_ready() noexcept { return false; }
                auto await_suspend(std::coroutine_handle<Promise> h) noexcept {
                    return h.promise().continuation_;
                }
                void await_resume() noexcept {}
            };
            return Awaitable{}; // 대기중인 코루틴을 재개.
        }
    };
    std::coroutine_handle<Promise> h_;
    explicit Task(Promise& p) noexcept
        : h_{ std::coroutine_handle<Promise>::from_promise(p) } {}
public:
    using promise_type = Promise;
    Task(Task&& t) noexcept : h_{ std::exchange(t.h_, {}) } {}
    ~Task() { if (h_) h_.destroy(); }

    // Awaitable interface
    // 결과가 준비되었는지(true) 또는 현재 코루틴을 일시 중단하고 결과가 준비될 때까지 기다려야 하는지 여부를 나타내는 bool을 반환합니다. 
    bool await_ready() { return false; }

    // await_ready()가 false를 반환하면 이 함수는 co_await를 실행한 코루틴에 대한 핸들로 호출됩니다.
    // 이 함수는 비동기 작업을 시작하고 작업이 완료된 후 코루틴을 재개할 때 트리거되는 알림을 구독할 수 있는 기회를 제공합니다.
    auto await_suspend(std::coroutine_handle<> c) {
        h_.promise().continuation_ = c;
        return h_;
    }

    // await_resume()은 결과(또는 오류)를 다시 코루틴으로 언팩킹 하는 역할을 하는 함수입니다. 
    // await_suspend()에 의해 시작된 작업 중에 오류가 발생한 경우 이 함수는 포착된 오류를 다시 발생시키거나 오류 코드를 반환할 수 있습니다. 
    // 전체 co_await 표현식의 결과는 await_resume()이 반환하는 것입니다.
    auto await_resume() -> T {
        auto& result = h_.promise().result_;
        if (result.index() == 1) {
            return std::get<1>(std::move(result));
        }
        else {
            std::rethrow_exception(std::get<2>(std::move(result)));
        }
    }
};

template <>
class [[nodiscard]] Task<void> {

    struct Promise {
        std::exception_ptr e_; // No std::variant, only exception
        std::coroutine_handle<> continuation_;
        auto get_return_object() noexcept { return Task{ *this }; }
        void return_void() {} // Instead of return_value() 
        void unhandled_exception() noexcept {
            e_ = std::current_exception();
        }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept {
            struct Awaitable {
                bool await_ready() noexcept { return false; }
                auto await_suspend(std::coroutine_handle<Promise> h) noexcept {
                    return h.promise().continuation_;
                }
                void await_resume() noexcept {}
            };
            return Awaitable{};
        }
    };
    std::coroutine_handle<Promise> h_;
    explicit Task(Promise& p) noexcept
        : h_{ std::coroutine_handle<Promise>::from_promise(p) } {}
public:
    using promise_type = Promise;
    Task(Task&& t) noexcept : h_{ std::exchange(t.h_, {}) } {}
    ~Task() { if (h_) h_.destroy(); }
    // Awaitable interface
    bool await_ready() { return false; }
    auto await_suspend(std::coroutine_handle<> c) {
        h_.promise().continuation_ = c;
        return h_;
    }
    void await_resume() {
        if (h_.promise().e_)
            std::rethrow_exception(h_.promise().e_);
    }
};

namespace detail { // Implementation detail
    template <typename T>
    class SyncWaitTask { // A helper class only used by sync_wait()
        struct Promise {
            T* value_{ nullptr };
            std::exception_ptr error_;
            std::binary_semaphore semaphore_{ 0 };
            SyncWaitTask get_return_object() noexcept {
                return SyncWaitTask{ *this };
            }
            void unhandled_exception() noexcept {
                error_ = std::current_exception();
            }
            auto yield_value(T&& x) noexcept { // Result has arrived
                value_ = std::addressof(x);
                return final_suspend();
            }
            auto initial_suspend() noexcept {
                return std::suspend_always{};
            }
            auto final_suspend() noexcept {
                struct Awaitable {
                    bool await_ready() noexcept { return false; }
                    void await_suspend(std::coroutine_handle<Promise> h) noexcept {
                        h.promise().semaphore_.release(); // Signal! 
                    }
                    void await_resume() noexcept {}
                };
                return Awaitable{};
            }
            void return_void() noexcept { assert(false); }
        };
        std::coroutine_handle<Promise> h_;
        explicit SyncWaitTask(Promise& p) noexcept
            : h_{ std::coroutine_handle<Promise>::from_promise(p) } {}
    public:
        using promise_type = Promise;

        SyncWaitTask(SyncWaitTask&& t) noexcept
            : h_{ std::exchange(t.h_, {}) } {}
        ~SyncWaitTask() { if (h_) h_.destroy(); }
        // Called from sync_wait(). Will block and retrieve the
        // value or error from the task passed to sync_wait()
        T&& get() {
            auto& p = h_.promise();
            h_.resume();
            p.semaphore_.acquire(); // Block until signal
            if (p.error_)
                std::rethrow_exception(p.error_);
            return static_cast<T&&>(*p.value_);
        }
        // No awaitable interface, this class will not be co_await:ed
    };
} // namespace detail

template<typename T>
using Result = decltype(std::declval<T&>().await_resume());
template <typename T>
Result<T> sync_wait(T&& task) {
    if constexpr (std::is_void_v<Result<T>>) {
        struct Empty {};
        auto coro = [&]() -> detail::SyncWaitTask<Empty> {
            co_await std::forward<T>(task);
            co_yield Empty{};
            assert(false);
        };
        coro().get();
    }
    else {
        auto coro = [&]() -> detail::SyncWaitTask<Result<T>> {
            // 다음으로 확장 됨. co_await promise.yield_value(some_value);
            co_yield co_await std::forward<T>(task);
            // This coroutine will be destroyed before it
            // has a chance to return.
            assert(false);
        };
        return coro().get();
    }
}