#ifndef REINA_VK_SAVEMANAGER_H
#define REINA_VK_SAVEMANAGER_H

#include <toml.hpp>
#include <string>

namespace reina::tools {
    struct SaveInfo {
        bool shouldSave;
        std::string filename;
    };

    class SaveManager {
    public:
        SaveManager() = default;
        explicit SaveManager(const toml::table& config);

        SaveInfo shouldSave(uint32_t samples, double time);

    private:
        std::vector<double> saveTimes;
        std::vector<int> saveSamples;
    };
}


#endif //REINA_VK_SAVEMANAGER_H
