#ifndef NTP_TIME_REFERENCE_H
#define NTP_TIME_REFERENCE_H

#include <stdint.h>

class NtpTimeReference
{
public:
    bool isValid() const { return valid; }

    uint64_t epochMillis(uint64_t uptimeMillis) const
    {
        return epochAtUpdateMillis + uptimeMillis - uptimeAtUpdateMillis;
    }

    int64_t update(uint32_t epochSeconds, uint64_t uptimeMillis)
    {
        const uint64_t newEpochMillis = static_cast<uint64_t>(epochSeconds) * 1000;
        const int64_t correctionMillis = valid
            ? static_cast<int64_t>(newEpochMillis) - static_cast<int64_t>(epochMillis(uptimeMillis))
            : 0;
        epochAtUpdateMillis = newEpochMillis;
        uptimeAtUpdateMillis = uptimeMillis;
        valid = true;
        return correctionMillis;
    }

private:
    bool valid = false;
    uint64_t epochAtUpdateMillis = 0;
    uint64_t uptimeAtUpdateMillis = 0;
};

#endif