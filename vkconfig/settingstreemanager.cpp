/*
 * Copyright (c) 2020 Valve Corporation
 * Copyright (c) 2020 LunarG, Inc.
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
 * This class takes a pointer to a treewidget and a profile
 * and creates a gui for displaying and editing those settings.
 *
 * Author: Richard S. Wright Jr. <richard@lunarg.com>
 */

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

#include "vulkanconfiguration.h"
#include "settingstreemanager.h"
#include "boolsettingwidget.h"
#include "enumsettingwidget.h"
#include "stringsettingwidget.h"
#include "filenamesettingwidget.h"
#include "foldersettingwidget.h"
#include "multienumsetting.h"


///////////////////////////////////////////////////////
// These correlate with CVulkanConfiguration::szCannedProfiles
#define KHRONOS_PRESET_STANDARD         0
#define KHRONOS_PRESET_BEST_PRACTICES   1
#define KHRONOS_PRESET_GPU_ASSIST       2
#define KHRONOS_PRESET_SHADER_PRINTF    3
#define KHRONOS_PRESET_LOW_OVERHEAD     4
#define KHRONOS_PRESET_USER_DEFINED     5

CSettingsTreeManager::CSettingsTreeManager()
    {
    pEditorTree = nullptr;
    pProfile = nullptr;
    pKhronosPresets = nullptr;
    pKhronosLayer = nullptr;
    pKhronosTree = nullptr;
    pAdvancedKhronosEditor = nullptr;
    pKhronosFileItem = nullptr;
    }

////////////////////////////////////////////////////////////////////////////////////
void CSettingsTreeManager::CreateGUI(QTreeWidget *pBuildTree, CProfileDef *pProfileDef)
    {
    CleanupGUI();

    pEditorTree = pBuildTree;
    pProfile = pProfileDef;

    pBuildTree->clear();

    // There will be one top level item for each layer
    for(int iLayer = 0; iLayer < pProfile->layers.size(); iLayer++) {
        QTreeWidgetItem *pLayerItem = new QTreeWidgetItem();
        pLayerItem->setText(0, pProfileDef->layers[iLayer]->name);
        pEditorTree->addTopLevelItem(pLayerItem);
        layerItems.push_back(pLayerItem);

        // Handle the case were we get off easy. No settings.
        if(pProfile->layers[iLayer]->layerSettings.size() == 0) {
            QTreeWidgetItem *pChild = new QTreeWidgetItem();
            pChild->setText(0, "No User Settings");
            pLayerItem->addChild(pChild);
            continue;
            }

        // There are settings.
        // Okay kid, this is where it gets complicated...
        // Is this Khronos? Khronos is special...
        if(pProfileDef->layers[iLayer]->name == QString("VK_LAYER_KHRONOS_validation")) {
            pKhronosLayer = pProfileDef->layers[iLayer];
            pKhronosTree = pLayerItem;
            BuildKhronosTree();
            continue;
            }

        // Generic is the only one left
        BuildGenericTree(pLayerItem, pProfileDef->layers[iLayer]);
        }


    ///////////////////////////////////////////////////////////////////
    // The last item is just the blacklisted layers
    if(pProfileDef->blacklistedLayers.isEmpty())
        return;

    QTreeWidgetItem* pBlackList = new QTreeWidgetItem();
    pBlackList->setText(0, "Disabled Layers");
    pBuildTree->addTopLevelItem(pBlackList);
    for(int i = 0; i < pProfileDef->blacklistedLayers.size(); i++) {
        QTreeWidgetItem *pChild = new QTreeWidgetItem();
        pChild->setText(0, pProfileDef->blacklistedLayers[i]);
        pBlackList->addChild(pChild);
        }

    pBuildTree->resizeColumnToContents(0);
    }

//////////////////////////////////////////////////////////////////////////
void CSettingsTreeManager::BuildKhronosTree(void)
    {
    QTreeWidgetItem* pItem = new QTreeWidgetItem();
    pItem->setText(0, "Validation Preset");
    pKhronosPresets = new QComboBox();
    pKhronosPresets->addItem("Standard");
    pKhronosPresets->addItem("Best Practices");
    pKhronosPresets->addItem("GPU Assisted");
    pKhronosPresets->addItem("Shader Printf");
    pKhronosPresets->addItem("Reduced-Overhead");
    pKhronosPresets->addItem("User Defined");

    connect(pKhronosPresets, SIGNAL(currentIndexChanged(int)), this, SLOT(khronosPresetChanged(int)));
    pKhronosTree->addChild(pItem);
    pEditorTree->setItemWidget(pItem, 1, pKhronosPresets);

    // This just finds the enables and disables
    pAdvancedKhronosEditor = new KhronosSettingsAdvanced(pEditorTree, pKhronosTree, pKhronosLayer->layerSettings);


    for(int iSetting = 0; iSetting < pKhronosLayer->layerSettings.size(); iSetting++) {
        QTreeWidgetItem *pChild = new QTreeWidgetItem();

        // Combobox - enum - just one thing
        if(pKhronosLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_EXCLUSIVE_LIST) {
            CEnumSettingWidget *pEnumWidget = new CEnumSettingWidget(pChild, pKhronosLayer->layerSettings[iSetting]);
            pKhronosTree->addChild(pChild);
            pEditorTree->setItemWidget(pChild, 1, pEnumWidget);
            }

        // Select a file?
        if(pKhronosLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_FILE) {
            CFilenameSettingWidget* pWidget = new CFilenameSettingWidget(pChild, pKhronosLayer->layerSettings[iSetting]);
            pKhronosTree->addChild(pChild);
            pEditorTree->setItemWidget(pChild, 1, pWidget);
            pKhronosFileItem = pChild;
            continue;
            }

        // Multi-enum - report flags only
        if(pKhronosLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_INCLUSIVE_LIST &&
                pKhronosLayer->layerSettings[iSetting]->settingsName == QString("report_flags")) {
            QTreeWidgetItem *pSubCategory = new QTreeWidgetItem;
            pSubCategory->setText(0, pKhronosLayer->layerSettings[iSetting]->settingsPrompt);
            pSubCategory->setToolTip(0, pKhronosLayer->layerSettings[iSetting]->settingsDesc);
            pKhronosTree->addChild(pSubCategory);

            for(int i = 0; i < pKhronosLayer->layerSettings[iSetting]->settingsListInclusiveValue.size(); i++) {
                QTreeWidgetItem *pChild = new QTreeWidgetItem();
                CMultiEnumSetting *pControl = new CMultiEnumSetting(pKhronosLayer->layerSettings[iSetting], pKhronosLayer->layerSettings[iSetting]->settingsListInclusiveValue[i]);
                pControl->setText(pKhronosLayer->layerSettings[iSetting]->settingsListInclusivePrompt[i]);
                pSubCategory->addChild(pChild);
                pEditorTree->setItemWidget(pChild, 0, pControl);
                }

            continue;
            }
        }

    pKhronosTree->addChild(pItem);
    }


void CSettingsTreeManager::BuildGenericTree(QTreeWidgetItem* pParent, CLayerFile *pLayer)
    {
    for(int iSetting = 0; iSetting < pLayer->layerSettings.size(); iSetting++) {
        QTreeWidgetItem *pSettingItem = new QTreeWidgetItem();

        // True false?
        if(pLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_BOOL) {
            CBoolSettingWidget *pBoolWidget = new CBoolSettingWidget(pLayer->layerSettings[iSetting]);
            pParent->addChild(pSettingItem);
            pEditorTree->setItemWidget(pSettingItem, 0, pBoolWidget);
            continue;
            }

        // True false? (with numeric output instead of text)
        if(pLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_BOOL_NUMERIC) {
            CBoolSettingWidget *pBoolWidget = new CBoolSettingWidget(pLayer->layerSettings[iSetting], true);
            pParent->addChild(pSettingItem);
            pEditorTree->setItemWidget(pSettingItem, 0, pBoolWidget);
            continue;
            }

        // Combobox - enum - just one thing
        if(pLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_EXCLUSIVE_LIST) {
            CEnumSettingWidget *pEnumWidget = new CEnumSettingWidget(pSettingItem, pLayer->layerSettings[iSetting]);
            pParent->addChild(pSettingItem);
            pEditorTree->setItemWidget(pSettingItem, 1, pEnumWidget);
            continue;
            }

        // Raw text field?
        if(pLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_STRING) {
            CStringSettingWidget *pStringWidget = new CStringSettingWidget(pSettingItem, pLayer->layerSettings[iSetting]);
            pParent->addChild(pSettingItem);
            pEditorTree->setItemWidget(pSettingItem, 1, pStringWidget);
            continue;
            }

        // Select a file?
        if(pLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_FILE) {
            CFilenameSettingWidget* pWidget = new CFilenameSettingWidget(pSettingItem, pLayer->layerSettings[iSetting]);
            pParent->addChild(pSettingItem);
            pEditorTree->setItemWidget(pSettingItem, 1, pWidget);
            fileWidgets.push_back(pSettingItem);
            continue;
            }


        // Save to folder?
        if(pLayer->layerSettings[iSetting]->settingsType == LAYER_SETTINGS_SAVE_FOLDER) {
            CFolderSettingWidget* pWidget = new CFolderSettingWidget(pSettingItem, pLayer->layerSettings[iSetting]);
            pParent->addChild(pSettingItem);
            pEditorTree->setItemWidget(pSettingItem, 1, pWidget);
            fileWidgets.push_back(pSettingItem);
            continue;
            }

        // Undefined... at least gracefuly display what the setting is
        pSettingItem->setText(0, pLayer->layerSettings[iSetting]->settingsPrompt);
        pSettingItem->setToolTip(0, pLayer->layerSettings[iSetting]->settingsDesc);
        pParent->addChild(pSettingItem);
        }
    }


////////////////////////////////////////////////////////////////////////////////////
/// The user has selected a preset for this layer
void CSettingsTreeManager::khronosPresetChanged(int nIndex)
    {
    // We really just don't care
    if(nIndex >= KHRONOS_PRESET_USER_DEFINED)
        return;

    // The easiest way to do this is to create a new profile, and copy the layer over
    QString preDefined = ":/resourcefiles/";
    preDefined += CVulkanConfiguration::szCannedProfiles[nIndex];
    preDefined += ".json";
    CProfileDef *pPatternProfile = CVulkanConfiguration::getVulkanConfig()->LoadProfile(preDefined);
    if(pPatternProfile == nullptr)
        return;

    // Copy it all into the real layer and delete it
    delete pProfile->layers[0];             // This is the Khronos layer we are deleting
    pProfile->layers[0] = new CLayerFile;   // Allocate a new one
    pKhronosLayer = pProfile->layers[0];    // Update Khronos pointer
    pPatternProfile->layers[0]->CopyLayer(pKhronosLayer);   // Copy pattern into the new layer
    delete pPatternProfile;                                 // Delete the pattern

    // Now we need to reload the Khronos tree item.
    pEditorTree->blockSignals(true);
    pEditorTree->setItemWidget(pKhronosFileItem, 1, nullptr);
    delete pAdvancedKhronosEditor;
    int nChildren = pKhronosTree->childCount();
    for(int i = 0; i < nChildren; i++)
        pKhronosTree->takeChild(0);

    BuildKhronosTree();
    pEditorTree->blockSignals(false);
    }

////////////////////////////////////////////////////////////////////////////////////
void CSettingsTreeManager::CleanupGUI(void)
    {
    if(pEditorTree == nullptr)
        return;

    if(pKhronosFileItem)
        pEditorTree->setItemWidget(pKhronosFileItem, 1, nullptr);

    pKhronosFileItem = nullptr;

    if(pAdvancedKhronosEditor)
        delete pAdvancedKhronosEditor;

    for(int i = 0; i < fileWidgets.size(); i++)
        pEditorTree->setItemWidget(fileWidgets[i], 1, nullptr);
    fileWidgets.clear();

    pEditorTree->clear();
    pEditorTree = nullptr;
    pProfile = nullptr;
    pKhronosPresets = nullptr;
    pKhronosLayer = nullptr;
    pKhronosTree = nullptr;
    pAdvancedKhronosEditor = nullptr;
    }
