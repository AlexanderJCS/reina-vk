#include "SaveManager.h"

#include <algorithm>


reina::tools::SaveManager::SaveManager(const toml::table& config) {
    auto saveSamplesArr = config.at_path("saving.save_on_samples").as_array();
    saveSamples = std::vector<int>(saveSamplesArr->size());

    for (int i = 0; i < saveSamplesArr->size(); i++) {
        saveSamples[i] = (*saveSamplesArr)[i].value<int>().value();
    }

    std::sort(saveSamples.begin(), saveSamples.end(), std::greater<>());

    auto saveTimesArr = config.at_path("saving.save_on_times").as_array();
    saveTimes = std::vector<double>(saveTimesArr->size());

    for (int i = 0; i < saveTimesArr->size(); i++) {
        saveTimes[i] = (*saveTimesArr)[i].value<double>().value();
    }

    std::sort(saveTimes.begin(), saveTimes.end(), std::greater<>());
}

reina::tools::SaveInfo reina::tools::SaveManager::shouldSave(uint32_t samples, double time) {
    if (!saveSamples.empty() && saveSamples.back() < samples) {
        int saveSample = saveSamples.back();
        saveSamples.pop_back();
        return { true, "output_" + std::to_string(saveSample) + "spp.png" };
    }

    if (!saveTimes.empty() && saveTimes.back() < time) {
        double saveTime = saveTimes.back();
        saveTimes.pop_back();
        return { true, "output_" + std::to_string(static_cast<int>(saveTime)) + "sec.png" };
    }

    return { false, "" };
}