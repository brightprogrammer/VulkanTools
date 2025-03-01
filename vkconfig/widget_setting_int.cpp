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

#include "widget_setting_int.h"
#include "widget_setting.h"

#include <QMessageBox>
#include <QTimer>
#include <QFontMetrics>

#include <cassert>

static const int MIN_FIELD_WIDTH = 48;

WidgetSettingInt::WidgetSettingInt(QTreeWidget* tree, QTreeWidgetItem* item, const SettingMetaInt& meta, SettingDataInt& data)
    : setting_meta(setting_meta), setting_data(data), field(new QLineEdit(this)) {
    assert(tree != nullptr);
    assert(item != nullptr);
    assert(&meta);
    assert(&data);

    const std::string unit = meta.unit.empty() ? "" : format(" (%s)", meta.unit.c_str());

    item->setText(0, (meta.label + unit).c_str());
    item->setFont(0, tree->font());
    item->setToolTip(0, meta.description.c_str());
    item->setSizeHint(0, QSize(0, ITEM_HEIGHT));

    this->field->setText(format("%d", data.value).c_str());
    this->field->setFont(tree->font());
    this->field->setToolTip(format("[%d, %d]", meta.min_value, meta.max_value).c_str());
    this->field->setAlignment(Qt::AlignRight);
    this->field->show();

    this->connect(this->field, SIGNAL(textEdited(const QString&)), this, SLOT(OnTextEdited(const QString&)));

    tree->setItemWidget(item, 0, this);
}

void WidgetSettingInt::Enable(bool enable) { this->field->setEnabled(enable); }

void WidgetSettingInt::FieldEditedCheck() {
    if (this->setting_data.value < setting_meta.min_value || this->setting_data.value > setting_meta.max_value) {
        const std::string text =
            format("'%s' is out of range. Use a value in the [%d, %d].", this->field->text().toStdString().c_str(),
                   this->setting_meta.min_value, this->setting_meta.max_value);
        const std::string into = format("Resetting to the setting default value: '%d'.", this->setting_meta.default_value);

        this->setting_data.value = this->setting_meta.default_value;
        this->field->setText(format("%d", this->setting_data.value).c_str());
        this->Resize();

        QMessageBox alert;
        alert.setWindowTitle(format("Invalid '%s' setting value", setting_meta.label.c_str()).c_str());
        alert.setText(text.c_str());
        alert.setInformativeText(into.c_str());
        alert.setStandardButtons(QMessageBox::Ok);
        alert.setIcon(QMessageBox::Critical);
        alert.exec();
    }
}

void WidgetSettingInt::Resize() {
    const QFontMetrics fm = this->field->fontMetrics();
    const int width = std::max(HorizontalAdvance(fm, this->field->text() + "00"), MIN_FIELD_WIDTH);

    const QRect button_rect = QRect(this->resize.width() - width, 0, width, this->resize.height());
    this->field->setGeometry(button_rect);
}

void WidgetSettingInt::resizeEvent(QResizeEvent* event) {
    this->resize = event->size();
    this->Resize();
}

void WidgetSettingInt::OnTextEdited(const QString& value) {
    if (value.isEmpty()) {
        this->setting_data.value = this->setting_meta.default_value;
        this->field->setText(format("%d", setting_data.value).c_str());
    } else if (!IsNumber(value.toStdString())) {
        this->setting_data.value = this->setting_meta.default_value;
        this->field->setText(format("%d", setting_data.value).c_str());

        QMessageBox alert;
        alert.setWindowTitle(format("Invalid '%s' setting value", setting_meta.label.c_str()).c_str());
        alert.setText("The setting input value is not a number. Please use digits [1-9] only.");
        alert.setInformativeText(format("Resetting to the setting default value: '%d'.", this->setting_meta.default_value).c_str());
        alert.setStandardButtons(QMessageBox::Ok);
        alert.setIcon(QMessageBox::Critical);
        alert.exec();
    } else {
        this->setting_data.value = std::atoi(value.toStdString().c_str());
    }

    this->Resize();
    QTimer::singleShot(1000, [this]() { FieldEditedCheck(); });

    emit itemChanged();
}
