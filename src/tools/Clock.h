#ifndef REINA_VK_CLOCK_H
#define REINA_VK_CLOCK_H

#include <map>
#include <string>
#include <vector>

#include "../window/Window.h"

namespace reina::tools {
    struct TimeEntries {
        unsigned int recordings = 0;
        double averageTime = 0;

        void addEntry(double timing);
    };

    /**
     * A rudimentary clock and profiler for keeping track of how long each frame takes, and how long each step takes
     * within one frame.
     */
    class Clock {
    public:
        static double getTime();

        void markFrame();
        void markCategory(const std::string& category);

        [[nodiscard]] unsigned int getFrameCount() const;
        [[nodiscard]] unsigned int getSamplesCount() const;

        [[nodiscard]] double getAverageFrameTime() const;
        [[nodiscard]] double getAverageCategoryTime(const std::string& category) const;

        std::string summary();
    private:
        double lastFrameTime = 0;
        TimeEntries frameTime;

        std::string lastCategory;
        double lastCategoryRecording = 0;
        std::map<std::string, TimeEntries> categoryTimes;
    };
}


#endif //REINA_VK_CLOCK_H
