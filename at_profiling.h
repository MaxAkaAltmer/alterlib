/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2025 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#pragma once

#include <type_traits>
#include "atime.h"

namespace alt {

template<typename T = std::size_t>
class profiling {
public:
    explicit profiling(string name = string(), int intervalCount = 10)
        : name_(name), threshold_(intervalCount)
    {
    }

    void calculate()
    {
        if (count_)
        {
            interval_average_ = time_acc_ / count_;

            if constexpr (!std::is_void_v<T>)
            {
                traffic_average_ = static_cast<double>(traffic_acc_) / count_;
                traffic_acc_ = T{};
            }

            last_sample_count_ = count_;
            time_acc_ = 0.0;
            count_ = 0;
        }
    }

    void push(double interval, T traffic = {})
    {
        count_++;
        time_acc_ += interval;

        if constexpr (!std::is_void_v<T>)
        {
            traffic_acc_ += traffic;
        }

        if (threshold_>0 && count_ >= threshold_)
        {
            calculate();
        }

        has_start_ = false;
    }

    void start(T traffic = {})
    {
        uint64 now = alt::time::uStamp();
        if (has_start_)
        {
            double interval_ms = (now - start_time_) / 1000.0;
            push(interval_ms, traffic);
        }

        start_time_ = now;
        has_start_ = true;
    }

    void end(T traffic = {})
    {
        if (has_start_)
        {
            uint64 now = alt::time::uStamp();
            double interval_ms = (now - start_time_) / 1000.0;
            push(interval_ms, traffic);
        }
    }

    void reset() {
        count_ = 0;
        time_acc_ = 0.0;
        interval_average_ = 0.0;
        last_sample_count_ = 0;
        has_start_ = false;

        if constexpr (!std::is_void_v<T>)
        {
            traffic_acc_ = T{};
            traffic_average_ = 0.0;
        }
    }

    double getAverage() const
    {
        return last_sample_count_ > 0 ? interval_average_ : 0.0;
    }

    double getTrafficAverage() const
    {
        if constexpr (!std::is_void_v<T>) {
            return last_sample_count_ > 0 ? traffic_average_ : 0.0;
        } else {
            return 0.0;
        }
    }

    T getTraffic() const
    {
        if constexpr (!std::is_void_v<T>) {
            return traffic_acc_;
        } else {
            return T{};
        }
    }

    const string& getName() const { return name_; }
    void setName(string newName) { name_ = newName; }

    int getCurrentCount() const { return count_; }
    double getAccumulatedTime() const { return time_acc_; }

    string toString() const
    {
        string rv(256,true);
        rv += "Profiling [" + name_ + "]: ";

        if (last_sample_count_ > 0)
        {
            rv += "avg = " + string::fromReal(interval_average_,4) + " ms";
        }
        else
        {
            rv += "avg = N/A";
        }

        rv += ", events = " + string::fromInt(last_sample_count_)
            + ", time = " + string::fromReal(time_acc_,4) + " ms";

        if constexpr (!std::is_void_v<T>)
        {
            if (last_sample_count_ > 0)
            {
                rv += ", traffic avg = " +string::fromInt(traffic_average_,2);
            }
            else
            {
                rv += ", traffic avg = N/A";
            }
        }

        return rv;
    }

    string toFpsString() const
    {
        if (last_sample_count_ == 0 || interval_average_ <= 0.0)
            return "FPS ["+name_+"]: N/A";

        double fps = 1000.0 / interval_average_;

        return "FPS ["+name_+"]: " + string::fromReal(fps,1);
    }

    string toSpeedString() const
    {
        if constexpr (!std::is_void_v<T>)
        {
            if (last_sample_count_ == 0 || traffic_average_ <= 0.0 || interval_average_ <= 0.0)
                return "Speed ["+name_+"]: N/A";

            double bps = traffic_average_ * (1000.0 / interval_average_);

            static const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s", "TB/s" };
            int unitIndex = 0;
            while (bps >= 1024.0 && unitIndex < 4)
            {
                bps /= 1024.0;
                ++unitIndex;
            }

            return "Speed ["+name_+"]: " + string::fromReal(bps, bps < 10 ? 2 : 1) + " " + units[unitIndex];
        }

        return "Speed ["+name_+"]: N/A";
    }

private:

    string name_;
    int threshold_;
    int count_ = 0;
    int last_sample_count_ = 0;

    double time_acc_ = 0.0;
    double interval_average_ = 0.0;

    double traffic_average_ = 0.0;
    [[maybe_unused]] T traffic_acc_{};

    uint64 start_time_ = 0;
    bool has_start_ = false;
};

} // namespace alt
