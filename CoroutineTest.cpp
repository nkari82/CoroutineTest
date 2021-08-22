// CoroutineTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <coroutine>
#include <semaphore>
#include <thread>
#include <vector>
#include <variant>
#include <cassert>
#include "CoroutineTest.h"


auto height() -> Task<int> { co_return 20; } // Dummy coroutines
auto width() -> Task<int> { co_return 30; }
auto area() -> Task<int> {
    co_return co_await height() * co_await width();
}

// 대략적인 flow
/*
// Pseudo code
auto&& a = expr; // Evaluate expr, a is the awaitable
if (!a.await_ready()) { // Not ready, wait for result
 a.await_suspend(h); // Handle to current coroutine
 // Suspend/resume happens here
}
auto result = a.await_resume();


co_yield some_value;
-> co_await promise.yield_value(some_value);
*/


auto coroutine() -> Resumable { // Initial suspend
    std::cout << "3 ";
    co_await std::suspend_always{}; // Suspend (explicit)
    std::cout << "5 ";
} // Final suspend then return


int main()
{
    // test 0
    {
        std::cout << "1 ";
        auto resumable = coroutine(); // Create coroutine state
        std::cout << "2 ";
        resumable.resume(); // Resume
        std::cout << "4 ";
        resumable.resume(); // Resume
        std::cout << "6 ";

    }
    
    // test 1
    {
        auto a = area();
        int value = sync_wait(a);
        std::cout << value; // Outputs: 600
    }

    // test 2
    {
        auto s = seq<int>();
        auto t = take_until<int>(s, 10);
        auto a = add<int>(t, 3);
        int sum = 0;
        for (auto&& v : a) {
            sum += v;
        }
    }

    std::cout << "Hello World!\n";
}