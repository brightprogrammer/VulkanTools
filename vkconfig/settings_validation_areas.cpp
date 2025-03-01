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
 * - Richard S. Wright Jr. <richard@lunarg.com>
 * - Christophe Riccio <christophe@lunarg.com>
 */

#include "widget_setting.h"
#include "settings_validation_areas.h"
#include "configurator.h"

#include "../vkconfig_core/version.h"
#include "../vkconfig_core/platform.h"
#include "../vkconfig_core/util.h"

#include <QSettings>
#include <QMessageBox>
#include <QCheckBox>

static const char *CORE_KEYS[] = {
    "VALIDATION_CHECK_DISABLE_IMAGE_LAYOUT_VALIDATION", "VALIDATION_CHECK_DISABLE_COMMAND_BUFFER_STATE",
    "VALIDATION_CHECK_DISABLE_OBJECT_IN_USE",           "VALIDATION_CHECK_DISABLE_QUERY_VALIDATION",
    "VALIDATION_CHECK_DISABLE_IDLE_DESCRIPTOR_SET",     "VK_VALIDATION_FEATURE_DISABLE_SHADERS_EXT",
    "VALIDATION_CHECK_DISABLE_PUSH_CONSTANT_RANGE"};

static const char *MISC_KEYS[] = {
    "VK_VALIDATION_FEATURE_DISABLE_THREAD_SAFETY_EXT", "VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT",
    "VK_VALIDATION_FEATURE_DISABLE_OBJECT_LIFETIMES_EXT", "VK_VALIDATION_FEATURE_DISABLE_API_PARAMETERS_EXT"};

static bool IsSupported(const std::vector<SettingEnumValue> &values, const char *key) {
    const SettingEnumValue *value = FindByKey(values, key);
    if (value == nullptr) return false;
    return (value->platform_flags & (1 << VKC_PLATFORM)) != 0;
}

SettingsValidationAreas::SettingsValidationAreas(QTreeWidget *main_tree, QTreeWidgetItem *parent, const Version &version,
                                                 const SettingMetaSet &settings_meta, SettingDataSet &settings_data)
    : _main_tree_widget(main_tree),
      _main_parent(parent),
      _core_box(nullptr),
      _synchronization_box(nullptr),
      _shader_based_box(nullptr),
      _gpu_assisted_box(nullptr),
      _gpu_assisted_radio(nullptr),
      _gpu_assisted_reserve_box(nullptr),
      _gpu_assisted_oob_box(nullptr),
      _debug_printf_box(nullptr),
      _debug_printf_radio(nullptr),
      _debug_printf_to_stdout(nullptr),
      _debug_printf_verbose(nullptr),
      _debug_printf_buffer_size(nullptr),
      _debug_printf_buffer_size_value(nullptr),
      _best_practices_box(nullptr),
      _best_practices_arm_box(nullptr),
      version(version),
      settings_meta(settings_meta),
      settings_data(settings_data) {
    assert(main_tree && parent);

    const SettingMetaFlags *setting_meta_disables = static_cast<const SettingMetaFlags *>(settings_meta.Get("disables"));
    assert(setting_meta_disables);

    // Core
    const SettingEnumValue *enum_core =
        FindByKey(setting_meta_disables->enum_values, "VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT");

    if (enum_core != nullptr) {
        const bool core_validation_enabled = !HasDisable("VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT");

        _core_box = new QTreeWidgetItem();
        _core_box->setText(0, enum_core->label.c_str());
        _core_box->setToolTip(0, enum_core->description.c_str());
        _core_box->setCheckState(0, core_validation_enabled ? Qt::Checked : Qt::Unchecked);
        parent->addChild(_core_box);

        for (std::size_t i = 0, n = countof(CORE_KEYS); i < n; i++) {
            const SettingEnumValue *value = FindByKey(setting_meta_disables->enum_values, CORE_KEYS[i]);
            if (value == nullptr) continue;

            const bool enabled = core_validation_enabled && !HasDisable(value->key.c_str());

            QTreeWidgetItem *item = this->AddItem(_core_box, value, enabled);
            if (item == nullptr) continue;

            ::EnableItem(item, core_validation_enabled);
            _core_box->addChild(item);
            _core_children_boxes.push_back(item);
        }
    }

    // Miscellaneous disables
    for (std::size_t i = 0, n = countof(MISC_KEYS); i < n; ++i) {
        const SettingEnumValue *enum_value = FindByKey(setting_meta_disables->enum_values, MISC_KEYS[i]);
        if (enum_value == nullptr) continue;

        const bool enabled = !HasDisable(enum_value->key.c_str());

        QTreeWidgetItem *item = this->AddItem(parent, enum_value, enabled);
        if (item == nullptr) continue;

        parent->addChild(item);
        _misc_boxes.push_back(item);
    }

    const SettingMetaFlags *setting_meta_enables = static_cast<const SettingMetaFlags *>(settings_meta.Get("enables"));
    assert(setting_meta_enables);

    // Shader-based enables
    const SettingEnumValue *enum_debug_printf =
        FindByKey(setting_meta_enables->enum_values, "VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT");
    const SettingEnumValue *enum_gpu_assisted =
        FindByKey(setting_meta_enables->enum_values, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT");

    const bool has_debug_printf = HasEnable("VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT");
    const bool has_gpu_assisted = HasEnable("VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT");

    if (enum_gpu_assisted != nullptr && enum_debug_printf != nullptr) {
        _shader_based_box = new QTreeWidgetItem();
        _shader_based_box->setText(0, "Shader-Based");
        _shader_based_box->setCheckState(0, has_debug_printf || has_gpu_assisted ? Qt::Checked : Qt::Unchecked);
        parent->addChild(_shader_based_box);

        // GPU assisted
        {
            _gpu_assisted_box = new QTreeWidgetItem();
            _gpu_assisted_box->setText(0, ("     " + enum_gpu_assisted->label).c_str());
            _gpu_assisted_box->setToolTip(0, enum_gpu_assisted->description.c_str());
            _shader_based_box->addChild(_gpu_assisted_box);

            _gpu_assisted_radio = new QRadioButton();
            _main_tree_widget->setItemWidget(_gpu_assisted_box, 0, _gpu_assisted_radio);

            const SettingEnumValue *enum_gpu_assisted_slot =
                FindByKey(setting_meta_enables->enum_values, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT");

            if (enum_gpu_assisted_slot != nullptr) {
                const bool enabled = HasEnable(enum_gpu_assisted_slot->key.c_str());
                _gpu_assisted_reserve_box = this->AddItem(_gpu_assisted_box, enum_gpu_assisted_slot, enabled);
            }

            const SettingMetaBool *meta_gpuav_buffer_oob = settings_meta.Get<SettingMetaBool>("gpuav_buffer_oob");

            if (meta_gpuav_buffer_oob != nullptr) {
                _gpu_assisted_oob_box = this->AddItem(_gpu_assisted_box, meta_gpuav_buffer_oob);
            }
        }

        // Debug Printf
        {
            _debug_printf_box = new QTreeWidgetItem();
            _debug_printf_box->setText(0, ("     " + enum_debug_printf->label).c_str());
            _debug_printf_box->setToolTip(0, enum_debug_printf->description.c_str());
            _shader_based_box->addChild(_debug_printf_box);

            _debug_printf_radio = new QRadioButton();
            _main_tree_widget->setItemWidget(_debug_printf_box, 0, _debug_printf_radio);

            const SettingMetaBool *meta_printf_to_stdout = settings_meta.Get<SettingMetaBool>("printf_to_stdout");
            if (meta_printf_to_stdout != nullptr) {
                _debug_printf_to_stdout = this->AddItem(_debug_printf_box, meta_printf_to_stdout);
            }

            const SettingMetaBool *meta_printf_verbose = settings_meta.Get<SettingMetaBool>("printf_verbose");
            if (meta_printf_verbose != nullptr) {
                _debug_printf_verbose = this->AddItem(_debug_printf_box, meta_printf_verbose);
            }

            const SettingMetaInt *meta_printf_buffer_size = settings_meta.Get<SettingMetaInt>("printf_buffer_size");
            if (meta_printf_buffer_size != nullptr) {
                SettingDataInt *data = settings_data.Get<SettingDataInt>(meta_printf_buffer_size->key.c_str());

                _debug_printf_buffer_size = new QTreeWidgetItem();
                _debug_printf_box->addChild(_debug_printf_buffer_size);
                _debug_printf_buffer_size_value =
                    new WidgetSettingInt(_main_tree_widget, _debug_printf_buffer_size, *meta_printf_buffer_size, *data);

                this->connect(_debug_printf_buffer_size_value, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            }
        }

        if (has_debug_printf || has_gpu_assisted) {
            _debug_printf_radio->setChecked(has_debug_printf);
            ::EnableItem(_debug_printf_to_stdout, has_debug_printf);
            ::EnableItem(_debug_printf_verbose, has_debug_printf);

            ::EnableItem(_debug_printf_buffer_size, has_debug_printf);
            if (_debug_printf_buffer_size != nullptr) {
                _debug_printf_buffer_size_value->setEnabled(has_debug_printf);
            }

            _gpu_assisted_radio->setChecked(has_gpu_assisted);
            ::EnableItem(_gpu_assisted_reserve_box, has_gpu_assisted);
            ::EnableItem(_gpu_assisted_oob_box, has_gpu_assisted);
        } else {
            _debug_printf_radio->setEnabled(false);
            ::EnableItem(_debug_printf_box, false);
            ::EnableItem(_debug_printf_to_stdout, false);
            ::EnableItem(_debug_printf_verbose, false);
            ::EnableItem(_debug_printf_buffer_size, false);
            if (_debug_printf_buffer_size != nullptr) {
                _debug_printf_buffer_size_value->setEnabled(false);
            }

            _gpu_assisted_radio->setEnabled(false);
            ::EnableItem(_gpu_assisted_box, false);
            ::EnableItem(_gpu_assisted_reserve_box, false);
            ::EnableItem(_gpu_assisted_oob_box, false);
        }

        connect(_gpu_assisted_radio, SIGNAL(toggled(bool)), this, SLOT(gpuToggled(bool)));
        connect(_debug_printf_radio, SIGNAL(toggled(bool)), this, SLOT(printfToggled(bool)));
        connect(_debug_printf_buffer_size_value, SIGNAL(textEdited(const QString &)), this,
                SLOT(printfBufferSizeEdited(const QString &)));
    } else if (enum_gpu_assisted != nullptr) {
        const bool enabled = HasEnable(enum_gpu_assisted->key.c_str());
        _gpu_assisted_box = this->AddItem(parent, enum_gpu_assisted, enabled);

        const SettingEnumValue *enum_gpu_assisted_slot =
            FindByKey(setting_meta_enables->enum_values, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT");

        if (enum_gpu_assisted_slot != nullptr) {
            const bool enabled = HasEnable(enum_gpu_assisted_slot->key.c_str());
            _gpu_assisted_reserve_box = this->AddItem(_gpu_assisted_box, enum_gpu_assisted_slot, enabled);
            ::EnableItem(_gpu_assisted_reserve_box, _gpu_assisted_box->checkState(0) == Qt::Checked);
        }
    }

    // Synchronization
    const SettingEnumValue *enum_synchronization =
        FindByKey(setting_meta_enables->enum_values, "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT");

    if (enum_synchronization != nullptr) {
        const bool enabled = HasEnable(enum_synchronization->key.c_str());
        _synchronization_box = this->AddItem(parent, enum_synchronization, enabled);
    }

    // Best Practices - one parent/child, but we want to be able to go back to these
    const SettingEnumValue *enum_best_practices =
        FindByKey(setting_meta_enables->enum_values, "VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT");

    if (enum_best_practices != nullptr) {
        const bool enabled = HasEnable(enum_best_practices->key.c_str());
        _best_practices_box = this->AddItem(parent, enum_best_practices, enabled);

        // ARM best practices
        const SettingEnumValue *enum_best_practices_arm =
            FindByKey(setting_meta_enables->enum_values, "VALIDATION_CHECK_ENABLE_VENDOR_SPECIFIC_ARM");

        if (enum_best_practices_arm != nullptr) {
            const bool enabled = HasEnable(enum_best_practices_arm->key.c_str());
            _best_practices_arm_box = this->AddItem(_best_practices_box, enum_best_practices_arm, enabled);
            ::EnableItem(_best_practices_arm_box, _best_practices_box->checkState(0) == Qt::Checked);
        }
    }

    // Events

    connect(_main_tree_widget, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(itemChanged(QTreeWidgetItem *, int)));
    connect(_main_tree_widget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(itemClicked(QTreeWidgetItem *, int)));
}

QTreeWidgetItem *SettingsValidationAreas::AddItem(QTreeWidgetItem *parent, const SettingEnumValue *enum_value, bool enabled) {
    if (enum_value == nullptr) return nullptr;
    if (enum_value->view == SETTING_VIEW_HIDDEN) return nullptr;
    if (!IsPlatformSupported(enum_value->platform_flags)) return nullptr;

    QTreeWidgetItem *child = new QTreeWidgetItem();
    child->setText(0, enum_value->label.c_str());
    child->setToolTip(0, enum_value->description.c_str());
    child->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
    parent->addChild(child);

    return child;
}

QTreeWidgetItem *SettingsValidationAreas::AddItem(QTreeWidgetItem *parent, const SettingMetaBool *setting) {
    if (setting == nullptr) return nullptr;
    if (setting->view == SETTING_VIEW_HIDDEN) return nullptr;
    if (!IsPlatformSupported(setting->platform_flags)) return nullptr;

    const SettingDataBool *data = settings_data.Get<SettingDataBool>(setting->key.c_str());

    QTreeWidgetItem *child = new QTreeWidgetItem();
    child->setText(0, setting->label.c_str());
    child->setToolTip(0, setting->description.c_str());
    child->setCheckState(0, data->value ? Qt::Checked : Qt::Unchecked);
    parent->addChild(child);

    return child;
}

void SettingsValidationAreas::itemClicked(QTreeWidgetItem *item, int column) {
    (void)item;
    (void)column;

    emit settingChanged();
}

/// Something was checked or unchecked
void SettingsValidationAreas::itemChanged(QTreeWidgetItem *item, int column) {
    if (column != 0) return;

    emit settingChanged();

    // Anything toggled needs to be selected in order to work well with the
    // information display.
    _main_tree_widget->setCurrentItem(item);

    // Best Practices
    _main_tree_widget->blockSignals(true);
    if (item == _best_practices_box && _best_practices_arm_box != nullptr) {
        if (_best_practices_arm_box && item->checkState(0) == Qt::Checked) {
            _best_practices_arm_box->setFlags(_best_practices_arm_box->flags() | Qt::ItemIsEnabled);
        } else {
            _best_practices_arm_box->setFlags(_best_practices_arm_box->flags() & ~Qt::ItemIsEnabled);
            _best_practices_arm_box->setCheckState(0, Qt::Unchecked);
        }
    }

    // Core Validation.
    if (item == _core_box) {
        const bool core_enabled = _core_box->checkState(0) == Qt::Checked;
        for (std::size_t i = 0, n = _core_children_boxes.size(); i < n; ++i) {
            _core_children_boxes[i]->setCheckState(0, core_enabled ? Qt::Checked : Qt::Unchecked);
            ::EnableItem(_core_children_boxes[i], core_enabled);
        }
    }

    // Shader based stuff
    if (item == _shader_based_box) {  // Just enable/disable the items below it
        if (_shader_based_box->checkState(0) == Qt::Checked) {
            _debug_printf_radio->setEnabled(true);
            ::EnableItem(_debug_printf_box, true);
            ::EnableItem(_debug_printf_to_stdout, true);
            ::EnableItem(_debug_printf_verbose, true);
            ::EnableItem(_debug_printf_buffer_size, true);
            if (_debug_printf_buffer_size != nullptr) _debug_printf_buffer_size_value->setEnabled(true);

            _gpu_assisted_radio->setEnabled(true);
            ::EnableItem(_gpu_assisted_box, true);
            ::EnableItem(_gpu_assisted_reserve_box, true);
            ::EnableItem(_gpu_assisted_oob_box, true);
        } else {
            _debug_printf_radio->setEnabled(false);
            ::EnableItem(_debug_printf_box, false);
            ::EnableItem(_debug_printf_to_stdout, false);
            ::EnableItem(_debug_printf_verbose, false);
            ::EnableItem(_debug_printf_buffer_size, false);
            if (_debug_printf_buffer_size != nullptr) _debug_printf_buffer_size_value->setEnabled(false);

            _gpu_assisted_radio->setEnabled(false);
            ::EnableItem(_gpu_assisted_box, false);
            ::EnableItem(_gpu_assisted_reserve_box, false);
            ::EnableItem(_gpu_assisted_oob_box, false);
        }
    }

    if (item == _gpu_assisted_box && _gpu_assisted_radio == nullptr) {
        ::EnableItem(_gpu_assisted_reserve_box, _gpu_assisted_box->checkState(0) == Qt::Checked);
        ::EnableItem(_gpu_assisted_oob_box, _gpu_assisted_box->checkState(0) == Qt::Checked);
    }

    // Debug printf or GPU based also enables/disables the checkbox for reserving a slot
    if (_debug_printf_radio != nullptr) {
        if (item == _debug_printf_box && _debug_printf_radio->isChecked()) {
            ::EnableItem(_gpu_assisted_reserve_box, false);
            ::EnableItem(_gpu_assisted_oob_box, false);
        }
    }

    if (_gpu_assisted_radio != nullptr) {
        if (item == _gpu_assisted_box && _gpu_assisted_radio->isChecked()) {
            ::EnableItem(_debug_printf_to_stdout, false);
            ::EnableItem(_debug_printf_verbose, false);
            ::EnableItem(_debug_printf_buffer_size, false);
            if (_debug_printf_buffer_size != nullptr) _debug_printf_buffer_size_value->setEnabled(false);
        }
    }

    _main_tree_widget->blockSignals(false);

    // Check for performance issues. There are three different variations, and I think
    // we should alert the user to all three exactly/explicitly to what they are

    const bool features_to_run_alone[] = {_core_box->checkState(0) == Qt::Checked,
                                          _synchronization_box ? _synchronization_box->checkState(0) == Qt::Checked : false,
                                          _best_practices_box->checkState(0) == Qt::Checked,
                                          _shader_based_box ? _shader_based_box->checkState(0) == Qt::Checked : false};

    int count_enabled_features = 0;
    for (std::size_t i = 0, n = countof(features_to_run_alone); i < n; ++i)
        count_enabled_features += features_to_run_alone[i] ? 1 : 0;

    if (count_enabled_features > 1) {
        QSettings settings;
        if (settings.value("VKCONFIG_WARN_CORE_SHADER_IGNORE").toBool() == false) {
            QMessageBox alert(_main_tree_widget);
            alert.setWindowTitle("High Validation Layer Overhead");
            alert.setText(
                "<i>Core Validation</i>, <i>Shader Based Validation</i>, <i>Synchronization Validation</i> and <i>Best Practices "
                "Warnings</i> require a state tracking object each.");
            alert.setInformativeText("Combining two of these options will result in high performance degradation.");
            alert.setIcon(QMessageBox::Warning);
            alert.setCheckBox(new QCheckBox("Do not show again."));
            alert.exec();
            if (alert.checkBox()->isChecked()) {
                settings.setValue("VKCONFIG_WARN_CORE_SHADER_IGNORE", true);
            }
        }
    }

    CollectSettings();
}

void SettingsValidationAreas::gpuToggled(bool toggle) {
    if (toggle) {
        ::EnableItem(_gpu_assisted_reserve_box, true);
        ::EnableItem(_gpu_assisted_oob_box, true);

        ::EnableItem(_debug_printf_to_stdout, false);
        ::EnableItem(_debug_printf_verbose, false);
        ::EnableItem(_debug_printf_buffer_size, false);
        if (_debug_printf_buffer_size != nullptr) {
            _debug_printf_buffer_size_value->setEnabled(false);
        }
    }

    CollectSettings();
    emit settingChanged();
}

void SettingsValidationAreas::printfToggled(bool toggle) {
    if (toggle) {
        ::EnableItem(_gpu_assisted_reserve_box, false);
        ::EnableItem(_gpu_assisted_oob_box, false);

        ::EnableItem(_debug_printf_to_stdout, true);
        ::EnableItem(_debug_printf_verbose, true);
        ::EnableItem(_debug_printf_buffer_size, true);
        if (_debug_printf_buffer_size != nullptr) {
            _debug_printf_buffer_size_value->setEnabled(true);
        }
    }

    CollectSettings();
    emit settingChanged();
}

void SettingsValidationAreas::OnSettingChanged() {
    CollectSettings();
    emit settingChanged();
}

bool SettingsValidationAreas::CollectSettings() {
    std::vector<std::string> enables;
    std::vector<std::string> disables;

    // GPU stuff
    if (_shader_based_box != nullptr) {
        if (_shader_based_box->checkState(0) == Qt::Checked) {
            if (_gpu_assisted_radio->isChecked()) {
                AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT");

                if (_gpu_assisted_reserve_box->checkState(0) == Qt::Checked) {
                    AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT");
                }
            } else {  // Debug printf must be checked
                AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT");
            }
        }
    } else if (_gpu_assisted_box != nullptr) {
        if (_gpu_assisted_box->checkState(0) == Qt::Checked) {
            AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT");

            if (_gpu_assisted_reserve_box->checkState(0) == Qt::Checked) {
                AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT");
            }
        }
    }

    StoreBoolSetting(_gpu_assisted_oob_box, "gpuav_buffer_oob");
    StoreBoolSetting(_debug_printf_to_stdout, "printf_to_stdout");
    StoreBoolSetting(_debug_printf_verbose, "printf_verbose");

    // Best practice enables
    if (_best_practices_box != nullptr) {
        if (_best_practices_box->checkState(0) == Qt::Checked)
            AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT");
    }

    if (_best_practices_arm_box != nullptr) {
        if (_best_practices_arm_box->checkState(0) == Qt::Checked)
            AppendString(enables, "VALIDATION_CHECK_ENABLE_VENDOR_SPECIFIC_ARM");
    }

    // Sync Validation
    if (_synchronization_box != nullptr) {
        if (_synchronization_box->checkState(0) == Qt::Checked) {
            AppendString(enables, "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT");
        }
    }

    // Misc disables
    for (std::size_t i = 0, n = this->_misc_boxes.size(); i < n; ++i) {
        if (this->_misc_boxes[i]->checkState(0) == Qt::Unchecked) AppendString(disables, MISC_KEYS[i]);
    }

    // Core disables. If unchecked, then individual ones might still be checked
    if (this->_core_box->checkState(0) == Qt::Checked) {
        for (std::size_t i = 0, n = this->_core_children_boxes.size(); i < n; ++i)
            if (this->_core_children_boxes[i]->checkState(0) == Qt::Unchecked) AppendString(disables, CORE_KEYS[i]);
    } else {  // Not checked, turn them all off
        AppendString(disables, "VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT");
    }

    static_cast<SettingDataFlags *>(settings_data.Get("disables"))->value = disables;
    static_cast<SettingDataFlags *>(settings_data.Get("enables"))->value = enables;

    return true;
}

bool SettingsValidationAreas::HasEnable(const char *token) const {
    return IsStringFound(static_cast<const SettingDataFlags *>(settings_data.Get("enables"))->value, token);
}

bool SettingsValidationAreas::HasDisable(const char *token) const {
    return IsStringFound(static_cast<const SettingDataFlags *>(settings_data.Get("disables"))->value, token);
}

void SettingsValidationAreas::StoreBoolSetting(QTreeWidgetItem *item, const char *key) {
    assert(key);
    assert(std::strcmp(key, "") != 0);

    if (item == nullptr) return;

    static_cast<SettingDataBool &>(*settings_data.Get(key)).value = item->checkState(0) == Qt::Checked;
}
