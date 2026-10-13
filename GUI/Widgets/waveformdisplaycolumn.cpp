//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.5.1
//
//  Copyright (c) 2020-2026 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <https://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include <iostream>
#include "multicolumndisplay.h"
#include "waveformdisplaycolumn.h"

namespace {
const QString CombinedPortsPrefix = QStringLiteral("Combined Ports A-");

QString combinedPortsId(const QString& lastPortLetter)
{
    return CombinedPortsPrefix + lastPortLetter;
}
}

WaveformDisplayColumn::WaveformDisplayColumn(int columnIndex_, WaveformDisplayManager* waveformManager,
                                             ControllerInterface* controllerInterface_, SystemState *state_,
                                             MultiColumnDisplay *parent_) :
    QWidget(parent_),
    parent(parent_),
    columnIndex(columnIndex_),
    controllerInterface(controllerInterface_),
    state(state_)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//    setFocusPolicy(Qt::StrongFocus);

    visible = true;
    deleteButtonEnabled = true;
    pinnedShown = true;

    portComboBox = new QComboBox(this);
    populatePortComboBox();

    waveformPlot = new MultiWaveformPlot(columnIndex, waveformManager, controllerInterface, state, this);
    connect(portComboBox, SIGNAL(currentIndexChanged(int)), waveformPlot, SLOT(updateFromState()));
    if (state->testMode->getValue()) {
        portComboBox->setFocusProxy(waveformPlot);
    }

    hideColumnButton = new QToolButton(this);
    hideColumnButton->setIcon(QIcon(":/images/collapseicon.png"));
    hideColumnButton->setToolTip(tr("Hide Column"));
    connect(hideColumnButton, SIGNAL(clicked()), this, SLOT(hideColumnSlot()));

    showColumnButton = new QToolButton(this);
    showColumnButton->setIcon(QIcon(":/images/expandicon.png"));
    showColumnButton->setToolTip(tr("Unhide Column"));
    showColumnButton->hide();
    connect(showColumnButton, SIGNAL(clicked()), this, SLOT(showColumnSlot()));

    moveLeftButton = new QToolButton(this);
    moveLeftButton->setIcon(QIcon(":/images/leftarrow.png"));
    moveLeftButton->setToolTip(tr("Move Column Left"));
    connect(moveLeftButton, SIGNAL(clicked()), this, SLOT(moveColumnLeft()));

    moveRightButton = new QToolButton(this);
    moveRightButton->setIcon(QIcon(":/images/rightarrow.png"));
    moveRightButton->setToolTip(tr("Move Column Right"));
    connect(moveRightButton, SIGNAL(clicked()), this, SLOT(moveColumnRight()));

    addColumnButton = new QToolButton(this);
    addColumnButton->setIcon(QIcon(":/images/plusicon.png"));
    addColumnButton->setToolTip(tr("Add Column"));
    connect(addColumnButton, SIGNAL(clicked()), this, SLOT(addColumn()));

    deleteColumnButton = new QToolButton(this);
    deleteColumnButton->setIcon(QIcon(":/images/minusicon.png"));
    deleteColumnButton->setToolTip(tr("Delete Column"));
    connect(deleteColumnButton, SIGNAL(clicked()), this, SLOT(deleteColumn()));

    if (state->testMode->getValue()) {
        pinWaveformButton = nullptr;
        showPinnedCheckBox = nullptr;
    } else {
        pinWaveformButton = new QToolButton(this);
        pinWaveformButton->setIcon(QIcon(":/images/thumbtackdownicon.png"));
        pinWaveformButton->setToolTip(tr("Add Pinned Waveform"));
        connect(pinWaveformButton, SIGNAL(clicked()), waveformPlot, SLOT(openWaveformSelectDialog()));

        showPinnedCheckBox = new QCheckBox(tr("show pinned"), this);
        showPinnedCheckBox->setChecked(true);
        connect(showPinnedCheckBox, SIGNAL(clicked()), this, SLOT(showPinnedWaveformsSlot()));
    }

    lessSpacingButton = new QToolButton(this);
    lessSpacingButton->setIcon(QIcon(":/images/lessspacingicon.png"));
    lessSpacingButton->setToolTip(tr("Less Spacing"));
    connect(lessSpacingButton, SIGNAL(clicked()), waveformPlot, SLOT(decreaseSpacing()));

    moreSpacingButton = new QToolButton(this);
    moreSpacingButton->setIcon(QIcon(":/images/morespacingicon.png"));
    moreSpacingButton->setToolTip(tr("More Spacing"));
    connect(moreSpacingButton, SIGNAL(clicked()), waveformPlot, SLOT(increaseSpacing()));

    QHBoxLayout* toolRow = new QHBoxLayout;
    toolRow->addWidget(showColumnButton);
    toolRow->addWidget(hideColumnButton);
    toolRow->addWidget(portComboBox);
    toolRow->addWidget(moveLeftButton);
    toolRow->addWidget(moveRightButton);
    if (!state->testMode->getValue()) {
        toolRow->addWidget(pinWaveformButton);
        toolRow->addWidget(showPinnedCheckBox);
    }
    toolRow->addWidget(lessSpacingButton);
    toolRow->addWidget(moreSpacingButton);
    toolRow->addStretch(1);
    toolRow->addWidget(addColumnButton);
    toolRow->addWidget(deleteColumnButton);

    verticalSpacer = new QSpacerItem(1, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(waveformPlot);
    mainLayout->addSpacerItem(verticalSpacer);
    mainLayout->addLayout(toolRow);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(1);

    setLayout(mainLayout);

    if (state->running) updateForRun();
}

WaveformDisplayColumn::~WaveformDisplayColumn()
{
    disconnect(this, nullptr, waveformPlot, nullptr);
    delete waveformPlot;    // Probably not necessary since WaveformPlot is a QWidget, but just to be safe.
}

void WaveformDisplayColumn::enableHideAndDelete(bool enable)
{
    hideColumnButton->setEnabled(enable);
    deleteColumnButton->setEnabled(enable & !state->running);
    deleteButtonEnabled = enable;
}

void WaveformDisplayColumn::setWaveformWidth(int width)
{
    parent->setWaveformWidth(width);
}

void WaveformDisplayColumn::hideColumnSlot()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    hideColumn();
}

void WaveformDisplayColumn::hideColumn()
{
    visible = false;

    showColumnButton->show();
    hideColumnButton->hide();

    portComboBox->hide();
    moveLeftButton->hide();
    moveRightButton->hide();
    if (!state->testMode->getValue()) {
        pinWaveformButton->hide();
        showPinnedCheckBox->hide();
    }
    lessSpacingButton->hide();
    moreSpacingButton->hide();
    addColumnButton->hide();
    deleteColumnButton->hide();
    waveformPlot->hide();

    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    verticalSpacer->changeSize(1, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    layout()->invalidate();
    parent->updateHideAndDeleteButtons();
}

void WaveformDisplayColumn::showColumnSlot()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    showColumn();
}

void WaveformDisplayColumn::showColumn()
{
    visible = true;

    showColumnButton->hide();
    hideColumnButton->show();

    portComboBox->show();
    moveLeftButton->show();
    moveRightButton->show();
    if (!state->testMode->getValue()) {
        pinWaveformButton->show();
        showPinnedCheckBox->show();
    }
    lessSpacingButton->show();
    moreSpacingButton->show();
    addColumnButton->show();
    deleteColumnButton->show();
    waveformPlot->show();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    verticalSpacer->changeSize(1, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout()->invalidate();
    parent->updateHideAndDeleteButtons();
}

void WaveformDisplayColumn::setSelectedPort(const QString& portName)
{
    for (int i = 0; i < portComboBox->count(); ++i) {
        if (portComboBox->itemData(i).toString() == portName ||
                portComboBox->itemText(i).section(" (", 0, 0) == portName) {
            portComboBox->setCurrentIndex(i);
            break;
        }
    }
}

QString WaveformDisplayColumn::getSelectedPort() const
{
    QString selectedPort = portComboBox->currentData().toString();
    if (selectedPort.isEmpty()) {
        selectedPort = portComboBox->currentText().section(" (", 0, 0);
    }
    return selectedPort;
}

QStringList WaveformDisplayColumn::getSelectedPorts() const
{
    if (!isCombinedPortSelection()) {
        return QStringList({getSelectedPort()});
    }

    const QString selectedPort = getSelectedPort();
    const QStringList combinedPorts = {QStringLiteral("Port A"), QStringLiteral("Port B"),
                                       QStringLiteral("Port C"), QStringLiteral("Port D")};
    const QString lastPortLetter = selectedPort.right(1);
    const int lastPortIndex = combinedPorts.indexOf(QStringLiteral("Port ") + lastPortLetter);
    if (lastPortIndex < 1) {
        return QStringList();
    }
    return combinedPorts.mid(0, lastPortIndex + 1);
}

bool WaveformDisplayColumn::isCombinedPortSelection() const
{
    return getSelectedPort().startsWith(CombinedPortsPrefix);
}

void WaveformDisplayColumn::populatePortComboBox()
{
    const QStringList combinedPorts = {QStringLiteral("Port A"), QStringLiteral("Port B"),
                                       QStringLiteral("Port C"), QStringLiteral("Port D")};
    QList<int> amplifierChannelCounts(combinedPorts.size(), 0);

    for (int i = 0; i < state->signalSources->numPortGroups(); ++i) {
        const SignalGroup* group = state->signalSources->portGroupByIndex(i);
        if (group->numChannels() == 0) continue;

        const QString groupName = group->getName();
        portComboBox->addItem(groupName + " (" + QString::number(group->numChannels(AmplifierSignal)) + " ch)",
                              groupName);
        const int amplifierChannelCount = group->numChannels(AmplifierSignal);
        const int combinedPortIndex = combinedPorts.indexOf(groupName);
        if (combinedPortIndex >= 0) {
            amplifierChannelCounts[combinedPortIndex] = amplifierChannelCount;
        }
    }

    int combinedChannelCount = amplifierChannelCounts.at(0);
    for (int lastPortIndex = 1; lastPortIndex < combinedPorts.size(); ++lastPortIndex) {
        if (combinedChannelCount == 0 || amplifierChannelCounts.at(lastPortIndex) == 0) {
            break;
        }
        combinedChannelCount += amplifierChannelCounts.at(lastPortIndex);
        const QString lastPortLetter = combinedPorts.at(lastPortIndex).right(1);
        portComboBox->addItem(tr("Ports A-%1 combined (%2 ch)")
                              .arg(lastPortLetter).arg(combinedChannelCount),
                              combinedPortsId(lastPortLetter));
        const int combinedIndex = portComboBox->count() - 1;
        portComboBox->setItemData(combinedIndex,
                                  tr("Display every amplifier channel from Ports A through %1 in this column.")
                                  .arg(lastPortLetter),
                                  Qt::ToolTipRole);
    }

    for (int i = 0; i < state->signalSources->numBaseGroups(); ++i) {
        const SignalGroup* group = state->signalSources->baseGroupByIndex(i);
        if (group->numChannels() > 0) {
            portComboBox->addItem(group->getName(), group->getName());
        }
    }
}

void WaveformDisplayColumn::updateForRun()
{
    deleteColumnButton->setEnabled(false);
}

void WaveformDisplayColumn::updateForStop()
{
    deleteColumnButton->setEnabled(deleteButtonEnabled);
}

void WaveformDisplayColumn::addColumn()
{
    parent->addColumn(columnIndex + 1);  // Add new column to the right of this column.
}

void WaveformDisplayColumn::deleteColumn()
{
    parent->deleteColumn(columnIndex);
}

void WaveformDisplayColumn::moveColumnLeft()
{
    parent->moveColumnLeft(columnIndex);
}

void WaveformDisplayColumn::moveColumnRight()
{
    parent->moveColumnRight(columnIndex);
}

void WaveformDisplayColumn::showPinnedWaveformsSlot()
{
    if (state->testMode->getValue()) return;
    state->signalSources->undoManager->pushStateToUndoStack();
    bool show = showPinnedCheckBox->checkState() != Qt::Unchecked;
    waveformPlot->showPinnedWaveforms(show);
    pinnedShown = show;
}

void WaveformDisplayColumn::setShowPinnedWaveforms(bool show)
{
    if (state->testMode->getValue()) return;
    showPinnedCheckBox->setChecked(show);
    waveformPlot->showPinnedWaveforms(show);
    pinnedShown = show;
}

void WaveformDisplayColumn::updatePortSelectionBox(bool switchToFirstPort)
{
    const QString selectedPort = getSelectedPort();
    const QSignalBlocker blocker(portComboBox);
    portComboBox->clear();
    populatePortComboBox();
    portComboBox->setCurrentIndex(0);  // Default to first port on new combo box list...
    if (!switchToFirstPort) {
        setSelectedPort(selectedPort); // ... but if previous selection is present, use that. Don't do this if we switch to first port.
    }
    waveformPlot->adjustToNewNumberOfPorts(portComboBox->count());
    // Caller should probably force updateFromState() after calling this function.
}

QString WaveformDisplayColumn::getColumnSettingsString() const
{
    QString columnSettings;
    const QString ParameterSeparator = ":";
    const QString ListSeparator = ",";

    columnSettings += visible ? "ShowColumn" : "HideColumn";
    columnSettings += ParameterSeparator;
    columnSettings += getSelectedPort();
    columnSettings += ParameterSeparator;
    columnSettings += waveformPlot->getScrollBarStateString();
    columnSettings += ParameterSeparator;
    columnSettings += pinnedShown ? "ShowPinned" : "HidePinned";
    columnSettings += ParameterSeparator;
    QStringList pinnedWaveNames = waveformPlot->getPinnedWaveNames();
    for (int i = 0; i < pinnedWaveNames.size(); ++i) {
        columnSettings += pinnedWaveNames[i];
        if (i < pinnedWaveNames.size() - 1) columnSettings += ListSeparator;
    }
    return columnSettings;
}

void WaveformDisplayColumn::restoreFromSettingsString(const QString& settings)
{
    const QString ParameterSeparator = ":";
    const QString ListSeparator = ",";

    bool columnVisible = !(settings.section(ParameterSeparator, 0, 0).toLower() == "hidecolumn");
    if (!columnVisible && visible) {
        hideColumn();
    } else if (columnVisible && !visible) {
        showColumn();
    }

    QString portSelection = settings.section(ParameterSeparator, 1, 1);
    setSelectedPort(portSelection);

    QString scrollBarState = settings.section(ParameterSeparator, 2, 2);
    waveformPlot->restoreScrollBarStateFromString(scrollBarState);

    bool showPinned = !(settings.section(ParameterSeparator, 3, 3).toLower() == "hidepinned");
    pinnedShown = showPinned;
    waveformPlot->showPinnedWaveforms(pinnedShown);

    waveformPlot->unpinAllWaveforms();
    QString pinnedWaveNames = settings.section(ParameterSeparator, 4, 4);
    int numPinned = 0;
    if (!pinnedWaveNames.isEmpty()) numPinned = pinnedWaveNames.count(ListSeparator) + 1;
    for (int i = 0; i < numPinned; ++i) {
        waveformPlot->addPinnedWaveform(pinnedWaveNames.section(ListSeparator, i, i));
    }
}
