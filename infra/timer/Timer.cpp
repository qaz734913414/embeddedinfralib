#include "infra/timer/Timer.hpp"
#include "infra/timer/TimerServiceManager.hpp"
#include <cassert>

namespace infra
{
    TimePoint Now(uint32_t timerServiceId)
    {
        return TimerServiceManager::Instance().GetTimerService(timerServiceId).Now();
    }

    Timer::Timer(uint32_t timerServiceId)
        : timerService(TimerServiceManager::Instance().GetTimerService(timerServiceId))
    {}

    Timer::~Timer()
    {
        Cancel();
    }

    void Timer::Cancel()
    {
        if (action)
            UnregisterSelf(Convert(nextTriggerTime));

        nextTriggerTime = Convert(TimePoint());
        action = nullptr;
    }

    bool Timer::Armed() const
    {
        return action != nullptr;
    }

    TimePoint Timer::NextTrigger() const
    {
        return Convert(nextTriggerTime);
    }

    const infra::Function<void()>& Timer::Action() const
    {
        return action;
    }

    void Timer::RegisterSelf()
    {
        timerService.RegisterTimer(*this);
    }

    void Timer::UnregisterSelf(TimePoint oldTriggerTime)
    {
        timerService.UnregisterTimer(*this, oldTriggerTime);
    }

    void Timer::UpdateTriggerTime(TimePoint oldTriggerTime)
    {
        timerService.UpdateTriggerTime(*this, oldTriggerTime);
    }

    Timer::UnalignedTimePoint Timer::Convert(TimePoint point) const
    {
        UnalignedTimePoint result;
        std::copy(reinterpret_cast<const uint32_t*>(&point), reinterpret_cast<const uint32_t*>(&point + 1), result.begin());
        return result;
    }

    TimePoint Timer::Convert(UnalignedTimePoint point) const
    {
        TimePoint result;
        std::copy(point.begin(), point.end(), reinterpret_cast<uint32_t*>(&result));
        return result;
    }

    void Timer::SetNextTriggerTime(TimePoint time, const infra::Function<void()>& action)
    {
        assert(action);
        TimePoint oldTriggerTime = Convert(nextTriggerTime);

        nextTriggerTime = Convert(time);

        if (!this->action)
            RegisterSelf();
        else
            UpdateTriggerTime(oldTriggerTime);

        this->action = action;
    }

    TimePoint Timer::Now() const
    {
        return timerService.Now();
    }

    Duration Timer::Resolution() const
    {
        return timerService.Resolution();
    }

    TimerSingleShot::TimerSingleShot(uint32_t timerServiceId)
        : Timer(timerServiceId)
    {}

    TimerSingleShot::TimerSingleShot(Duration duration, const infra::Function<void()>& aAction, uint32_t timerServiceId)
        : Timer(timerServiceId)
    {
        Start(duration, aAction);
    }

    void TimerSingleShot::Start(TimePoint time, const infra::Function<void()>& action)
    {
        SetNextTriggerTime(time + Resolution(), action);
    }

    void TimerSingleShot::Start(Duration duration, const infra::Function<void()>& action)
    {
        SetNextTriggerTime(Now() + duration + Resolution(), action);
    }

    void TimerSingleShot::ComputeNextTriggerTime()
    {
        Cancel();
    }

    TimerRepeating::TimerRepeating(uint32_t timerServiceId)
        : Timer(timerServiceId)
    {}

    TimerRepeating::TimerRepeating(Duration duration, const infra::Function<void()>& aAction, uint32_t timerServiceId)
        : Timer(timerServiceId)
    {
        Start(duration, aAction);
    }

    TimerRepeating::TimerRepeating(Duration duration, const infra::Function<void()>& aAction, TriggerImmediately, uint32_t timerServiceId)
        : Timer(timerServiceId)
    {
        Start(duration, aAction, triggerImmediately);
    }

    void TimerRepeating::Start(Duration duration, const infra::Function<void()>& action)
    {
        SetNextTriggerTime(Now() + Resolution(), action);  // Initialize NextTrigger() for ComputeNextTriggerTime
        SetDuration(duration);
    }

    void TimerRepeating::Start(Duration duration, const infra::Function<void()>& action, TriggerImmediately)
    {
        Start(duration, action);
        action();
    }

    void TimerRepeating::ComputeNextTriggerTime()
    {
        TimePoint now = std::max(Now(), NextTrigger());
        Duration diff = (now - NextTrigger()) % triggerPeriod;

        SetNextTriggerTime(now - diff + triggerPeriod, Action());
    }

    void TimerRepeating::SetDuration(Duration duration)
    {
        triggerPeriod = duration;

        ComputeNextTriggerTime();
    }
}
