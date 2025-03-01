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

#pragma once

#include "../vkconfig_core/setting_data.h"
#include "../vkconfig_core/setting_meta.h"

#include <QWidget>
#include <QTreeWidgetItem>
#include <QResizeEvent>
#include <QStringList>
#include <QCompleter>
#include <QLineEdit>
#include <QPushButton>

class WidgetSettingList : public QWidget {
    Q_OBJECT

   public:
    explicit WidgetSettingList(QTreeWidget *tree, QTreeWidgetItem *item, const SettingMetaList &meta, SettingDataList &data);

   public Q_SLOTS:
    void OnAddCompleted(const QString &value);
    void OnButtonPressed();
    void OnTextEdited(const QString &value);
    void OnSettingChanged();
    void OnItemSelected(const QString &value);

   Q_SIGNALS:
    void itemSelected(const QString &value);
    void itemChanged();

   private:
    WidgetSettingList(const WidgetSettingList &) = delete;
    WidgetSettingList &operator=(const WidgetSettingList &) = delete;

    virtual void resizeEvent(QResizeEvent *event) override;
    virtual bool eventFilter(QObject *target, QEvent *event) override;

    void Resize();
    void AddElement(const std::string &key);
    void ResetCompleter();

    const SettingMetaList &meta;
    SettingDataList &data;

    QTreeWidget *tree;
    QTreeWidgetItem *item;
    QCompleter *search;
    QLineEdit *field;
    QPushButton *add_button;
    QSize size;

    std::vector<NumberOrString> list;
};
