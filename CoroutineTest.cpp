// CoroutineTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <coroutine>


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
int main()
{
    auto s = seq<int>();
    auto t = take_until<int>(s, 10);
    auto a = add<int>(t, 3);
    int sum = 0;
    for (auto&& v : a) {
        sum += v;
    }
    return sum; // returns 75
    std::cout << "Hello World!\n";
}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
