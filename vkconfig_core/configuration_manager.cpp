/*
 * Copyright (c) 2020-2021 Valve Corporation
 * Copyright (c) 2020-2021 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors:
 * - Christophe Riccio <christophe@lunarg.com>
 */

#include "util.h"
#include "path_manager.h"
#include "configuration_manager.h"
#include "override.h"

#include <QMessageBox>
#include <QFileInfoList>

static const char *SUPPORTED_CONFIG_FILES[] = {"_2_2_1", "_2_2", ""};

ConfigurationManager::ConfigurationManager(const PathManager &path_manager, Environment &environment)
    : active_configuration(nullptr), path_manager(path_manager), environment(environment) {}

ConfigurationManager::~ConfigurationManager() {}

void ConfigurationManager::LoadAllConfigurations(const std::vector<Layer> &available_layers) {
    this->available_configurations.clear();
    this->active_configuration = nullptr;

    // If this is the first time, we need to create the initial set of
    // configuration files.
    if (environment.first_run) {
        RemoveConfigurationFiles();
        LoadDefaultConfigurations(available_layers);

        environment.first_run = false;
    }

    const std::string base_config_path = GetPath(BUILTIN_PATH_CONFIG_REF);

    for (std::size_t i = 0, n = countof(SUPPORTED_CONFIG_FILES); i < n; ++i) {
        const std::string path = base_config_path + SUPPORTED_CONFIG_FILES[i];
        LoadConfigurationsPath(available_layers, path.c_str());
    }

    RefreshConfiguration(available_layers);
}

void ConfigurationManager::LoadDefaultConfigurations(const std::vector<Layer> &available_layers) {
    const QFileInfoList &configuration_files = GetJSONFiles(":/configurations/");

    for (int i = 0, n = configuration_files.size(); i < n; ++i) {
        Configuration configuration;
        const bool result = configuration.Load(available_layers, configuration_files[i].absoluteFilePath().toStdString());
        assert(result);

        if (!IsPlatformSupported(configuration.platform_flags)) continue;

        OrderParameter(configuration.parameters, available_layers);

        Configuration *found_configuration = FindByKey(this->available_configurations, configuration.key.c_str());
        if (found_configuration == nullptr) {
            this->available_configurations.push_back(configuration);
        }
    }

    this->SortConfigurations();
}

// Discard old built-in configurations that have been replaced by new configurations
static bool IsConfigurationExcluded(const char *filename) {
    static const char *EXCLUDED_FILENAMES[] = {"Validation - Synchronization (Alpha).json",
                                               "Validation - Standard.json",
                                               "Validation - Reduced-Overhead.json",
                                               "Validation - GPU-Assisted.json",
                                               "Validation - Debug Printf.json",
                                               "Validation - Shader Printf.json",
                                               "Validation - Best Practices.json",
                                               "Frame Capture - Range (F5 to start and to stop).json",
                                               "Frame Capture - Range (F10 to start and to stop).json",
                                               "Frame Capture - First two frames.json",
                                               "applist.json"};

    for (std::size_t i = 0, n = countof(EXCLUDED_FILENAMES); i < n; ++i) {
        if (std::strcmp(EXCLUDED_FILENAMES[i], filename) == 0) {
            return true;
        }
    }

    return false;
}

void ConfigurationManager::SortConfigurations() {
    this->active_configuration = nullptr;

    struct Compare {
        bool operator()(const Configuration &a, const Configuration &b) const {
            const bool result = a.key < b.key;
            return result;
        }
    };

    std::sort(this->available_configurations.begin(), this->available_configurations.end(), Compare());
}

void ConfigurationManager::LoadConfigurationsPath(const std::vector<Layer> &available_layers, const char *path) {
    const QFileInfoList &configuration_files = GetJSONFiles(path);
    for (int i = 0, n = configuration_files.size(); i < n; ++i) {
        const QFileInfo &info = configuration_files[i];
        if (SUPPORT_LAYER_CONFIG_2_0_3 && IsConfigurationExcluded(info.fileName().toStdString().c_str())) continue;

        Configuration configuration;
        const bool result = configuration.Load(available_layers, info.absoluteFilePath().toStdString());

        if (!result) continue;

        if (FindByKey(available_configurations, configuration.key.c_str()) != nullptr) continue;

        OrderParameter(configuration.parameters, available_layers);
        available_configurations.push_back(configuration);
    }
}

void ConfigurationManager::SaveAllConfigurations(const std::vector<Layer> &available_layers) {
    for (std::size_t i = 0, n = available_configurations.size(); i < n; ++i) {
        const std::string path = GetPath(BUILTIN_PATH_CONFIG_LAST) + "/" + available_configurations[i].key + ".json";
        available_configurations[i].Save(available_layers, path.c_str());
    }
}

Configuration &ConfigurationManager::CreateConfiguration(const std::vector<Layer> &available_layers,
                                                         const std::string &configuration_name, bool duplicate) {
    Configuration *duplicate_configuration = FindByKey(available_configurations, configuration_name.c_str());

    Configuration new_configuration = duplicate_configuration != nullptr && duplicate ? *duplicate_configuration : Configuration();
    new_configuration.key = MakeConfigurationName(available_configurations, configuration_name);

    const std::string path = GetPath(BUILTIN_PATH_CONFIG_LAST) + "/" + new_configuration.key + ".json";
    new_configuration.Save(available_layers, path.c_str());

    // Reload from file to workaround the lack of SettingSet copy support
    Configuration configuration;
    const bool result = configuration.Load(available_layers, path.c_str());

    this->available_configurations.push_back(configuration);
    this->SortConfigurations();

    SetActiveConfiguration(available_layers, configuration.key);

    return *FindByKey(this->available_configurations, configuration.key.c_str());
}

bool ConfigurationManager::HasFile(const Configuration &configuration) const {
    const std::string base_path = GetPath(BUILTIN_PATH_CONFIG_REF);

    for (std::size_t i = 0, n = countof(SUPPORTED_CONFIG_FILES); i < n; ++i) {
        const std::string path = base_path + SUPPORTED_CONFIG_FILES[i] + "/" + configuration.key + ".json";

        std::FILE *file = std::fopen(path.c_str(), "r");
        if (file) {
            std::fclose(file);
            return true;
        }
    }
    return false;
}

void ConfigurationManager::RemoveConfigurationFiles() {
    const std::string base_path = GetPath(BUILTIN_PATH_CONFIG_REF);

    for (std::size_t i = 0, n = countof(SUPPORTED_CONFIG_FILES); i < n; ++i) {
        const std::string path = base_path + SUPPORTED_CONFIG_FILES[i];

        const QFileInfoList &configuration_files = GetJSONFiles(path.c_str());
        for (int i = 0, n = configuration_files.size(); i < n; ++i) {
            std::remove(configuration_files[i].filePath().toStdString().c_str());
        }
    }
}

void ConfigurationManager::RemoveConfigurationFile(const std::string &key) {
    const std::string base_path = GetPath(BUILTIN_PATH_CONFIG_REF);

    for (std::size_t i = 0, n = countof(SUPPORTED_CONFIG_FILES); i < n; ++i) {
        const std::string path = base_path + SUPPORTED_CONFIG_FILES[i];

        const QFileInfoList &configuration_files = GetJSONFiles(path.c_str());
        for (int j = 0, o = configuration_files.size(); j < o; ++j) {
            const std::string filename = configuration_files[j].baseName().toStdString();
            if (filename == key) {
                std::remove(configuration_files[j].filePath().toStdString().c_str());
            }
        }
    }
}

void ConfigurationManager::RemoveConfiguration(const std::vector<Layer> &available_layers, const std::string &configuration_name) {
    assert(!configuration_name.empty());

    // Not the active configuration
    if (active_configuration != nullptr) {
        if (active_configuration->key == configuration_name.c_str()) {
            SetActiveConfiguration(available_layers, nullptr);
        }
    }

    RemoveConfigurationFile(configuration_name.c_str());

    // Update the configuration in the list
    std::vector<Configuration> updated_configurations;

    for (std::size_t i = 0, n = available_configurations.size(); i < n; ++i) {
        if (available_configurations[i].key == configuration_name) continue;
        updated_configurations.push_back(available_configurations[i]);
    }

    std::swap(updated_configurations, available_configurations);

    SetActiveConfiguration(available_layers, nullptr);
}

void ConfigurationManager::SetActiveConfiguration(const std::vector<Layer> &available_layers,
                                                  const std::string &configuration_name) {
    assert(!configuration_name.empty());

    Configuration *configuration = FindByKey(available_configurations, configuration_name.c_str());
    assert(configuration != nullptr);

    SetActiveConfiguration(available_layers, configuration);
}

void ConfigurationManager::SetActiveConfiguration(const std::vector<Layer> &available_layers, Configuration *active_configuration) {
    this->active_configuration = active_configuration;

    bool surrender = false;
    if (this->active_configuration != nullptr) {
        assert(!this->active_configuration->key.empty());
        environment.Set(ACTIVE_CONFIGURATION, this->active_configuration->key.c_str());
        surrender = !this->active_configuration->HasOverride();
    } else {
        this->active_configuration = nullptr;
        surrender = true;
    }

    if (surrender) {
        SurrenderConfiguration(environment);
    } else {
        assert(this->active_configuration != nullptr);
        OverrideConfiguration(environment, available_layers, *active_configuration);
    }
}

void ConfigurationManager::RefreshConfiguration(const std::vector<Layer> &available_layers) {
    const std::string active_configuration_name = environment.Get(ACTIVE_CONFIGURATION);

    if (!active_configuration_name.empty() && environment.UseOverride()) {
        Configuration *active_configuration = FindByKey(available_configurations, active_configuration_name.c_str());
        if (active_configuration == nullptr) {
            environment.Set(ACTIVE_CONFIGURATION, "");
        }
        SetActiveConfiguration(available_layers, active_configuration);
    } else {
        SetActiveConfiguration(available_layers, nullptr);
    }
}

bool ConfigurationManager::HasActiveConfiguration(const std::vector<Layer> &available_layers) const {
    if (this->active_configuration != nullptr)
        return !HasMissingLayer(this->active_configuration->parameters, available_layers) &&
               this->active_configuration->HasOverride();
    else
        return false;
}

void ConfigurationManager::ImportConfiguration(const std::vector<Layer> &available_layers, const std::string &full_import_path) {
    assert(!full_import_path.empty());

    Configuration configuration;
    if (!configuration.Load(available_layers, full_import_path)) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Import of Layers Configuration error");
        msg.setText("Cannot access the source configuration file.");
        msg.setInformativeText(full_import_path.c_str());
        msg.exec();
        return;
    }

    configuration.key = MakeConfigurationName(this->available_configurations, configuration.key + " (Imported)");
    this->available_configurations.push_back(configuration);
    this->SortConfigurations();
    SetActiveConfiguration(available_layers, configuration.key.c_str());
}

void ConfigurationManager::ExportConfiguration(const std::vector<Layer> &available_layers, const std::string &full_export_path,
                                               const std::string &configuration_name) {
    assert(!configuration_name.empty());
    assert(!full_export_path.empty());

    Configuration *configuration = FindByKey(available_configurations, configuration_name.c_str());
    assert(configuration);

    if (!configuration->Save(available_layers, full_export_path, true)) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Export of Layers Configuration error");
        msg.setText("Cannot create the destination configuration file.");
        msg.setInformativeText(full_export_path.c_str());
        msg.exec();
    }
}

void ConfigurationManager::ResetDefaultsConfigurations(const std::vector<Layer> &available_layers) {
    // Clear the current configuration as we may be about to remove it.
    SetActiveConfiguration(available_layers, nullptr);

    environment.Reset(Environment::DEFAULT);

    // Now we need to kind of restart everything
    LoadAllConfigurations(available_layers);
}

void ConfigurationManager::ReloadDefaultsConfigurations(const std::vector<Layer> &available_layers) {
    // Clear the current configuration as we may be about to remove it.
    SetActiveConfiguration(available_layers, nullptr);

    // Now we need to kind of restart everything
    LoadDefaultConfigurations(available_layers);

    RefreshConfiguration(available_layers);
}

void ConfigurationManager::FirstDefaultsConfigurations(const std::vector<Layer> &available_layers) {
    const QFileInfoList &configuration_files = GetJSONFiles(":/configurations/");

    for (int i = 0, n = configuration_files.size(); i < n; ++i) {
        if (environment.IsDefaultConfigurationInit(configuration_files[i].baseName().toStdString())) {
            continue;
        }

        environment.InitDefaultConfiguration(configuration_files[i].baseName().toStdString());

        Configuration configuration;
        const bool result = configuration.Load(available_layers, configuration_files[i].absoluteFilePath().toStdString());
        assert(result);

        if (!IsPlatformSupported(configuration.platform_flags)) {
            continue;
        }

        if (FindByKey(available_configurations, configuration.key.c_str()) != nullptr) {
            continue;
        }

        OrderParameter(configuration.parameters, available_layers);
        available_configurations.push_back(configuration);
    }

    RefreshConfiguration(available_layers);
}
