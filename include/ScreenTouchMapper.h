#pragma once

#include <cstdint>

enum class ScreenTouchRegion : uint8_t {
    LEFT,
    CENTER,
    RIGHT,
};

class ScreenTouchMapper {
public:
    static ScreenTouchRegion horizontalRegion(int x, int width)
    {
        // Use three stable screen zones. Exact pixel positions are not needed.
        if (width <= 0) {
            return ScreenTouchRegion::CENTER;
        }
        if (x < width / 3) {
            return ScreenTouchRegion::LEFT;
        }
        if (x >= (width * 2) / 3) {
            return ScreenTouchRegion::RIGHT;
        }
        return ScreenTouchRegion::CENTER;
    }

    static bool isDebugCorner(int x, int y, int width, int height)
    {
        return width > 0 && height > 0 &&
               x >= (width * 4) / 5 && y < height / 4;
    }

    static bool isCalibrationArea(int x, int y, int width, int height)
    {
        return width > 0 && height > 0 &&
               x >= width / 3 && x < (width * 2) / 3 &&
               y >= height / 4 && y < (height * 3) / 4;
    }

    static int steppedYawTarget(int currentYaw, ScreenTouchRegion region,
                                int step, int maximum)
    {
        // Accumulate small yaw steps and stop at the configured safe limit.
        if (step < 0) {
            step = -step;
        }
        if (maximum < 0) {
            maximum = -maximum;
        }

        int64_t next = currentYaw;
        if (region == ScreenTouchRegion::LEFT) {
            next += step;
        } else if (region == ScreenTouchRegion::RIGHT) {
            next -= step;
        }
        if (next > maximum) {
            next = maximum;
        } else if (next < -maximum) {
            next = -maximum;
        }
        return static_cast<int>(next);
    }
};
