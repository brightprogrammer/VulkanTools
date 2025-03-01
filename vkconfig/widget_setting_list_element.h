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

#pragma once

#include "../vkconfig_core/setting_data.h"
#include "../vkconfig_core/setting_meta.h"

#include <QWidget>
#include <QResizeEvent>
#include <QCheckBox>
#include <QPushButton>
#include <QTreeWidgetItem>

class WidgetSettingListElement : public QCheckBox {
    Q_OBJECT
   public:
    explicit WidgetSettingListElement(QTreeWidget* tree, const SettingMetaList& meta, SettingDataList& data,
                                      const std::string& element);

   public Q_SLOTS:
    void OnButtonClicked();
    void OnItemChecked(bool checked);

   Q_SIGNALS:
    void itemSelected(const QString& value);
    void itemChanged();

   private:
    WidgetSettingListElement(const WidgetSettingListElement&) = delete;
    WidgetSettingListElement& operator=(const WidgetSettingListElement&) = delete;

    void resizeEvent(QResizeEvent* event) override;

    bool GetChecked() const;

    const SettingMetaList& meta;
    SettingDataList& data;
    std::string element;
    QPushButton* button;
};
