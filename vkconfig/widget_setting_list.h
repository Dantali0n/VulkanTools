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

#include "../vkconfig_core/setting_list.h"

#include "widget_setting.h"

#include <QResizeEvent>
#include <QStringList>
#include <QCompleter>
#include <QLineEdit>
#include <QPushButton>

class WidgetSettingList : public WidgetSettingBase {
    Q_OBJECT

   public:
    explicit WidgetSettingList(QTreeWidget *tree, QTreeWidgetItem *item, const SettingMetaList &meta, SettingDataSet &data_set);

    void Refresh(RefreshAreas refresh_areas) override;

   public Q_SLOTS:
    void OnCompleted(const QString &value);
    void OnElementAppended();
    void OnTextEdited(const QString &value);
    void OnSettingChanged();
    void OnElementRemoved(const QString &value);
    void OnElementRejected();

   Q_SIGNALS:
    void itemSelected(const QString &value);
    void itemChanged();

   protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *target, QEvent *event) override;

   private:
    void Resize();
    void AddElement(EnabledNumberOrString &element);
    void ResetCompleter();

    SettingDataList &data();

    const SettingMetaList &meta;
    SettingDataSet &data_set;

    QCompleter *search;
    QLineEdit *field;
    QPushButton *add_button;
    QSize size;

    std::vector<NumberOrString> list;
    std::vector<EnabledNumberOrString> value_cached;
};
