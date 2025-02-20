#include "Clock.h"

#include <sstream>
#include "../../polyglot/common.h"

void reina::tools::TimeEntries::addEntry(double timing) {
    if (recordings == 0) {
        averageTime = timing;
        recordings++;
        return;
    }

    averageTime = (averageTime * recordings + timing) / (recordings + 1);
    recordings++;
}

reina::tools::Clock::Clock() : creationTime(getTime()) {}

double reina::tools::Clock::getTime() {
    return glfwGetTime();
}

double reina::tools::Clock::getTimeFromCreation() {
    return getTime() - creationTime;
}

void reina::tools::Clock::markFrame() {
    // don't simply compare with 0 because floating point errors.
    // is it likely there's a floating point error here? no. do I want to risk it? also no.
    if (lastFrameTime < 0.000001) {
        lastFrameTime = getTime();
        return;
    }

    double time = getTime();
    frameTime.addEntry(time - lastFrameTime);
    lastFrameTime = time;
}

void reina::tools::Clock::markCategory(const std::string& category) {
    if (!categoryTimes.contains(category)) {
        categoryTimes.emplace(category, TimeEntries{});
    }

    if (lastCategoryRecording < 0.000001) {
        lastCategoryRecording = getTime();
        lastCategory = category;
        return;
    }

    double time = getTime();
    categoryTimes.at(lastCategory).addEntry(time - lastCategoryRecording);
    lastCategory = category;
    lastCategoryRecording = time;
}

std::string reina::tools::Clock::summary() {
    std::ostringstream oss;
    oss << "Timer age: " << getTimeFromCreation() << "s\n";
    oss << "Samples: " << getSampleCount() << "\n";
    oss << "Average frame time: " << frameTime.averageTime * 1000 << "ms\n";
    oss << "Average time per spp: " << frameTime.averageTime * 1000 / SAMPLES_PER_PIXEL << "ms\n";

    for (auto & time : categoryTimes) {
        oss << "Average category time | " << time.first << ": " << time.second.averageTime * 1000 << "ms\n";
    }

    return oss.str();
}

unsigned int reina::tools::Clock::getFrameCount() const {
    return frameTime.recordings;
}

unsigned int reina::tools::Clock::getSampleCount() const {
    return getFrameCount() * SAMPLES_PER_PIXEL;
}

double reina::tools::Clock::getAverageFrameTime() const {
    return frameTime.averageTime;
}

double reina::tools::Clock::getAverageCategoryTime(const std::string& category) const {
    return categoryTimes.at(category).averageTime;
}
