#pragma once

class Timer
{
public:
    Timer();

    float TotalTime() const;   // in seconds
    float DeltaTime() const;   // in seconds
    float DeltaTimeMS() const; // in ms

    void Reset(); // Call before message loop.
    void Tick();  // Call every frame.

private:
    double m_SecondsPerCount;
    double m_DeltaTime;

    __int64 m_BaseTime;
    __int64 m_CurrTime;
    __int64 m_PrevTime;
};
