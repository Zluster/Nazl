#include "timer.h"
#include <iostream>
#include <cassert>
#include <atomic>

void testSingleShotTimer()
{
    std::cout << "Testing single-shot timer..." << std::endl;
    Nazl::TimerMgr timerMgr;
    std::atomic<bool> callbackInvoked(false);

    auto timer = timerMgr.createTimer(std::chrono::seconds(1), [&callbackInvoked]()
    {
        std::cout << "Single-shot timer callback invoked!" << std::endl;
        callbackInvoked = true;
    });

    timerMgr.start(timer);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    assert(callbackInvoked);
    assert(!timer.isValid());
    assert(timerMgr.getActiveTimerCount() == 0);
    std::cout << "Single-shot timer test passed." << std::endl;
}

void testPeriodicTimer()
{
    std::cout << "Testing periodic timer..." << std::endl;
    Nazl::TimerMgr timerMgr;
    std::atomic<int> callCount(0);

    auto timer = timerMgr.createTimer(std::chrono::milliseconds(500), [&callCount]()
    {
        callCount++;
        std::cout << "Periodic timer callback invoked! Count: " << callCount << std::endl;
    }, true);

    timerMgr.start(timer);
    std::this_thread::sleep_for(std::chrono::milliseconds(2250));
    timerMgr.stop(timer);

    assert(callCount >= 4);
    assert(!timer.isValid());
    assert(timerMgr.getActiveTimerCount() == 0);
    std::cout << "Periodic timer test passed." << std::endl;
}

void testMultipleTimers()
{
    std::cout << "Testing multiple timers..." << std::endl;
    Nazl::TimerMgr timerMgr;
    std::atomic<int> timersFired(0);

    auto createTimerCallback = [&timersFired](int id)
    {
        return [&timersFired, id]()
        {
            std::cout << "Timer " << id << " fired!" << std::endl;
            timersFired++;
        };
    };

    auto timer1 = timerMgr.createTimer(std::chrono::milliseconds(500), createTimerCallback(1));
    auto timer2 = timerMgr.createTimer(std::chrono::milliseconds(750), createTimerCallback(2));
    auto timer3 = timerMgr.createTimer(std::chrono::seconds(1), createTimerCallback(3));

    timerMgr.start(timer1);
    timerMgr.start(timer2);
    timerMgr.start(timer3);

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    assert(timersFired == 3);
    assert(!timer1.isValid() && !timer2.isValid() && !timer3.isValid());
    assert(timerMgr.getActiveTimerCount() == 0);
    std::cout << "Multiple timers test passed." << std::endl;
}


int main()
{
    testSingleShotTimer();
    std::cout << std::endl;
    testPeriodicTimer();
    std::cout << std::endl;
    testMultipleTimers();
    std::cout << std::endl;
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
