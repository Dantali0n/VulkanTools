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

class WidgetSettingSearch : public QWidget {
    Q_OBJECT

   public:
    explicit WidgetSettingSearch(QTreeWidget *tree, QTreeWidgetItem *item, const SettingMetaList &meta, SettingDataList &data);

   public Q_SLOTS:
    void addButtonPressed();
    void addCompleted(const QString &addedItem);
    void addToSearchList(const QString &newItem);

   Q_SIGNALS:
    void itemSelected(const QString &textSelected);
    void itemChanged();

   private:
    WidgetSettingSearch(const WidgetSettingSearch &) = delete;
    WidgetSettingSearch &operator=(const WidgetSettingSearch &) = delete;

    virtual void resizeEvent(QResizeEvent *event) override;
    virtual bool eventFilter(QObject *target, QEvent *event) override;

    void ResetCompleter();
    void AddChildItem(const char *label, bool checked);

    const SettingMetaList &meta;
    SettingDataList &data;

    QTreeWidgetItem *item;
    QCompleter *search;
    QLineEdit *field;
    QPushButton *add_button;

    std::vector<std::string> list;
    std::vector<bool> enabled;
};
