#include "Timer.h"

Timer::Timer()
    : m_SecondsPerCount(0.0), m_DeltaTime(0.0f), m_BaseTime(0),
      m_CurrTime(0), m_PrevTime(0)
{
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER *)&countsPerSec);
    m_SecondsPerCount = 1.0 / (double)countsPerSec;
}

// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
float Timer::TotalTime() const
{
    return (float)((m_CurrTime - m_BaseTime) * m_SecondsPerCount);
}

float Timer::DeltaTime() const
{
    return (float)m_DeltaTime;
}

float Timer::DeltaTimeMS() const
{
    return (float)m_DeltaTime * 1000.0f;
}

void Timer::Reset()
{
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER *)&currTime);
    m_BaseTime = currTime;
    m_PrevTime = currTime;
}

void Timer::Tick()
{
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER *)&currTime);
    m_CurrTime = currTime;

    // Time difference between this frame and the previous.
    m_DeltaTime = (m_CurrTime - m_PrevTime) * m_SecondsPerCount;

    // Prepare for next frame.
    m_PrevTime = m_CurrTime;

    // Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the
    // processor goes into a power save mode or we get shuffled to another
    // processor, then m_DeltaTime can be negative.
    if (m_DeltaTime < 0.0)
    {
        m_DeltaTime = 0.0;
    }
}