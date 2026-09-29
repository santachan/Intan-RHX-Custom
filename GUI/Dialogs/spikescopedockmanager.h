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
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
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

#ifndef SPIKESCOPEDOCKMANAGER_H
#define SPIKESCOPEDOCKMANAGER_H

#include <QList>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSet>

class SpikeSortingDialog;

class SpikeScopeDockManager
{
public:
    explicit SpikeScopeDockManager(int snapDistancePixels = 24);

    void registerWindow(SpikeSortingDialog* window);
    void unregisterWindow(SpikeSortingDialog* window);
    void detachWindow(SpikeSortingDialog* window);

    void windowMoved(SpikeSortingDialog* window, const QPoint& oldPosition, const QPoint& newPosition);
    void windowResized(SpikeSortingDialog* window);

private:
    enum DockSide {
        DockLeft,
        DockRight,
        DockTop,
        DockBottom
    };

    struct DockLink {
        QPointer<SpikeSortingDialog> first;
        QPointer<SpikeSortingDialog> second;
        DockSide secondSideOfFirst;
    };

    struct SnapCandidate {
        SpikeSortingDialog* movingWindow = nullptr;
        SpikeSortingDialog* targetWindow = nullptr;
        DockSide movingSideOfTarget = DockRight;
        QPoint translation;
        int score = 0;
        bool valid = false;
    };

    QList<QPointer<SpikeSortingDialog>> windows;
    QList<DockLink> links;
    QSet<SpikeSortingDialog*> snapSuppressedWindows;
    int snapDistance;
    bool applyingLayout;

    void prune();
    QList<SpikeSortingDialog*> connectedComponent(SpikeSortingDialog* root) const;
    bool directlyDocked(SpikeSortingDialog* window) const;
    void updateDetachButtons();

    void moveComponent(const QList<SpikeSortingDialog*>& component, const QPoint& delta,
                       SpikeSortingDialog* alreadyMoved = nullptr);
    void layoutComponentFrom(SpikeSortingDialog* root);
    void positionWindow(SpikeSortingDialog* movingWindow, SpikeSortingDialog* fixedWindow, DockSide side);

    SnapCandidate findBestSnap(const QList<SpikeSortingDialog*>& movingComponent) const;
    void addLink(SpikeSortingDialog* targetWindow, SpikeSortingDialog* movingWindow, DockSide movingSideOfTarget);
    bool shouldReleaseSnapSuppression(SpikeSortingDialog* window) const;

    static DockSide oppositeSide(DockSide side);
    static int rangeOverlap(int firstStart, int firstEnd, int secondStart, int secondEnd);
    static QPoint translationForSnap(const QRect& movingFrame, const QRect& targetFrame, DockSide side);
};

#endif // SPIKESCOPEDOCKMANAGER_H
