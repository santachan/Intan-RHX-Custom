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

#include <QtGlobal>

#include "spikescopedockmanager.h"
#include "spikesortingdialog.h"

SpikeScopeDockManager::SpikeScopeDockManager(int snapDistancePixels) :
    snapDistance(snapDistancePixels),
    applyingLayout(false)
{
}

void SpikeScopeDockManager::registerWindow(SpikeSortingDialog* window)
{
    if (!window) return;

    prune();
    for (const auto& registeredWindow : windows) {
        if (registeredWindow == window) return;
    }

    windows.append(window);
    updateWindowChrome();
}

void SpikeScopeDockManager::unregisterWindow(SpikeSortingDialog* window)
{
    if (!window) return;

    for (int i = links.size() - 1; i >= 0; --i) {
        if (links.at(i).first == window || links.at(i).second == window) {
            links.removeAt(i);
        }
    }
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows.at(i).isNull() || windows.at(i) == window) {
            windows.removeAt(i);
        }
    }
    snapSuppressedWindows.remove(window);
    updateWindowChrome();
}

void SpikeScopeDockManager::detachWindow(SpikeSortingDialog* window)
{
    if (!window) return;

    prune();
    for (int i = links.size() - 1; i >= 0; --i) {
        if (links.at(i).first == window || links.at(i).second == window) {
            links.removeAt(i);
        }
    }

    // Do not immediately re-snap while the user is dragging a just-detached window away.
    snapSuppressedWindows.insert(window);
    updateWindowChrome();
}

void SpikeScopeDockManager::clearScopesInGroup(SpikeSortingDialog* window)
{
    if (!window) return;

    prune();
    const QList<SpikeSortingDialog*> component = connectedComponent(window);
    for (SpikeSortingDialog* groupedWindow : component) {
        if (groupedWindow) groupedWindow->clearScopeDisplay();
    }
}

void SpikeScopeDockManager::windowMoved(SpikeSortingDialog* window, const QPoint& oldPosition, const QPoint& newPosition)
{
    if (!window || applyingLayout || oldPosition == newPosition) return;

    prune();
    QList<SpikeSortingDialog*> component = connectedComponent(window);
    moveComponent(component, newPosition - oldPosition, window);

    if (snapSuppressedWindows.contains(window)) {
        if (shouldReleaseSnapSuppression(window)) {
            snapSuppressedWindows.remove(window);
        }
        return;
    }

    SnapCandidate candidate = findBestSnap(component);
    if (!candidate.valid) return;

    moveComponent(component, candidate.translation);
    addLink(candidate.targetWindow, candidate.movingWindow, candidate.movingSideOfTarget);
}

void SpikeScopeDockManager::windowResized(SpikeSortingDialog* window)
{
    if (!window || applyingLayout) return;

    prune();
    if (directlyDocked(window)) {
        layoutComponentFrom(window);
    }
}

void SpikeScopeDockManager::prune()
{
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows.at(i).isNull()) windows.removeAt(i);
    }
    for (int i = links.size() - 1; i >= 0; --i) {
        if (links.at(i).first.isNull() || links.at(i).second.isNull()) {
            links.removeAt(i);
        }
    }
}

QList<SpikeSortingDialog*> SpikeScopeDockManager::connectedComponent(SpikeSortingDialog* root) const
{
    QList<SpikeSortingDialog*> result;
    if (!root) return result;

    QSet<SpikeSortingDialog*> visited;
    result.append(root);
    visited.insert(root);

    for (int index = 0; index < result.size(); ++index) {
        SpikeSortingDialog* current = result.at(index);
        for (const DockLink& link : links) {
            SpikeSortingDialog* neighbor = nullptr;
            if (link.first == current) neighbor = link.second;
            else if (link.second == current) neighbor = link.first;

            if (neighbor && !visited.contains(neighbor)) {
                visited.insert(neighbor);
                result.append(neighbor);
            }
        }
    }

    return result;
}

bool SpikeScopeDockManager::directlyDocked(SpikeSortingDialog* window) const
{
    for (const DockLink& link : links) {
        if (link.first == window || link.second == window) return true;
    }
    return false;
}

void SpikeScopeDockManager::updateWindowChrome(SpikeSortingDialog* preferredPanelOwner)
{
    QSet<SpikeSortingDialog*> visited;

    for (const auto& windowPointer : windows) {
        SpikeSortingDialog* window = windowPointer;
        if (!window || visited.contains(window)) continue;

        const QList<SpikeSortingDialog*> component = connectedComponent(window);
        for (SpikeSortingDialog* groupedWindow : component) visited.insert(groupedWindow);

        const bool docked = component.size() > 1;
        SpikeSortingDialog* panelOwner = nullptr;
        if (docked && preferredPanelOwner && component.contains(preferredPanelOwner)) {
            panelOwner = preferredPanelOwner;
        }
        if (docked && !panelOwner) {
            for (SpikeSortingDialog* groupedWindow : component) {
                if (groupedWindow && groupedWindow->isControlPanelVisible()) {
                    panelOwner = groupedWindow;
                    break;
                }
            }
        }
        if (docked && !panelOwner && !component.isEmpty()) panelOwner = component.first();

        for (SpikeSortingDialog* groupedWindow : component) {
            if (!groupedWindow) continue;
            groupedWindow->setDocked(docked);
            groupedWindow->setControlPanelVisible(!docked || groupedWindow == panelOwner);
        }
    }
}

void SpikeScopeDockManager::moveComponent(const QList<SpikeSortingDialog*>& component, const QPoint& delta,
                                          SpikeSortingDialog* alreadyMoved)
{
    if (delta.isNull()) return;

    applyingLayout = true;
    for (SpikeSortingDialog* window : component) {
        if (window && window != alreadyMoved) {
            window->move(window->pos() + delta);
        }
    }
    applyingLayout = false;
}

void SpikeScopeDockManager::layoutComponentFrom(SpikeSortingDialog* root)
{
    applyingLayout = true;

    QList<SpikeSortingDialog*> queue;
    QSet<SpikeSortingDialog*> visited;
    queue.append(root);
    visited.insert(root);

    for (int index = 0; index < queue.size(); ++index) {
        SpikeSortingDialog* fixedWindow = queue.at(index);
        for (const DockLink& link : links) {
            SpikeSortingDialog* movingWindow = nullptr;
            DockSide movingSide = DockRight;

            if (link.first == fixedWindow) {
                movingWindow = link.second;
                movingSide = link.secondSideOfFirst;
            } else if (link.second == fixedWindow) {
                movingWindow = link.first;
                movingSide = oppositeSide(link.secondSideOfFirst);
            }

            if (movingWindow && !visited.contains(movingWindow)) {
                positionWindow(movingWindow, fixedWindow, movingSide);
                visited.insert(movingWindow);
                queue.append(movingWindow);
            }
        }
    }

    applyingLayout = false;
}

void SpikeScopeDockManager::positionWindow(SpikeSortingDialog* movingWindow, SpikeSortingDialog* fixedWindow, DockSide side)
{
    if (!movingWindow || !fixedWindow) return;

    QPoint delta = translationForSnap(movingWindow->frameGeometry(), fixedWindow->frameGeometry(), side);
    movingWindow->move(movingWindow->pos() + delta);
}

SpikeScopeDockManager::SnapCandidate SpikeScopeDockManager::findBestSnap(
        const QList<SpikeSortingDialog*>& movingComponent) const
{
    SnapCandidate best;
    QSet<SpikeSortingDialog*> movingSet;
    for (SpikeSortingDialog* window : movingComponent) movingSet.insert(window);

    for (SpikeSortingDialog* movingWindow : movingComponent) {
        if (!movingWindow || !movingWindow->isVisible() || movingWindow->isMinimized()) continue;
        const QRect movingFrame = movingWindow->frameGeometry();

        for (const auto& targetPointer : windows) {
            SpikeSortingDialog* targetWindow = targetPointer;
            if (!targetWindow || movingSet.contains(targetWindow) || !targetWindow->isVisible() ||
                    targetWindow->isMinimized()) continue;

            const QRect targetFrame = targetWindow->frameGeometry();
            const int verticalOverlap = rangeOverlap(movingFrame.top(), movingFrame.bottom(),
                                                     targetFrame.top(), targetFrame.bottom());
            const int horizontalOverlap = rangeOverlap(movingFrame.left(), movingFrame.right(),
                                                       targetFrame.left(), targetFrame.right());
            const int requiredVerticalOverlap = qMin(80, qMin(movingFrame.height(), targetFrame.height()) / 3);
            const int requiredHorizontalOverlap = qMin(80, qMin(movingFrame.width(), targetFrame.width()) / 3);

            struct SideDistance {
                DockSide side;
                int distance;
                bool enoughOverlap;
            };

            const SideDistance distances[] = {
                { DockRight, qAbs(movingFrame.left() - targetFrame.right() - 1),
                  verticalOverlap >= requiredVerticalOverlap },
                { DockLeft, qAbs(movingFrame.right() - targetFrame.left() + 1),
                  verticalOverlap >= requiredVerticalOverlap },
                { DockBottom, qAbs(movingFrame.top() - targetFrame.bottom() - 1),
                  horizontalOverlap >= requiredHorizontalOverlap },
                { DockTop, qAbs(movingFrame.bottom() - targetFrame.top() + 1),
                  horizontalOverlap >= requiredHorizontalOverlap }
            };

            for (const SideDistance& sideDistance : distances) {
                if (!sideDistance.enoughOverlap || sideDistance.distance > snapDistance) continue;
                if (sideOccupied(targetWindow, sideDistance.side) ||
                        sideOccupied(movingWindow, oppositeSide(sideDistance.side))) continue;

                QPoint translation = translationForSnap(movingFrame, targetFrame, sideDistance.side);
                const int maximumAlignmentDistance = snapDistance * 3;
                if ((sideDistance.side == DockLeft || sideDistance.side == DockRight) &&
                        qAbs(translation.y()) > maximumAlignmentDistance) continue;
                if ((sideDistance.side == DockTop || sideDistance.side == DockBottom) &&
                        qAbs(translation.x()) > maximumAlignmentDistance) continue;
                if (componentsWouldOverlap(movingComponent, targetWindow, translation)) continue;

                int alignmentPenalty = (sideDistance.side == DockLeft || sideDistance.side == DockRight) ?
                            qAbs(translation.y()) / 4 : qAbs(translation.x()) / 4;
                int score = sideDistance.distance + alignmentPenalty;

                if (!best.valid || score < best.score) {
                    best.movingWindow = movingWindow;
                    best.targetWindow = targetWindow;
                    best.movingSideOfTarget = sideDistance.side;
                    best.translation = translation;
                    best.score = score;
                    best.valid = true;
                }
            }
        }
    }

    return best;
}

void SpikeScopeDockManager::addLink(SpikeSortingDialog* targetWindow, SpikeSortingDialog* movingWindow,
                                    DockSide movingSideOfTarget)
{
    if (!targetWindow || !movingWindow || targetWindow == movingWindow) return;

    SpikeSortingDialog* preferredPanelOwner = targetWindow;
    const QList<SpikeSortingDialog*> targetComponent = connectedComponent(targetWindow);
    for (SpikeSortingDialog* groupedWindow : targetComponent) {
        if (groupedWindow && groupedWindow->isControlPanelVisible()) {
            preferredPanelOwner = groupedWindow;
            break;
        }
    }

    DockLink link;
    link.first = targetWindow;
    link.second = movingWindow;
    link.secondSideOfFirst = movingSideOfTarget;
    links.append(link);
    updateWindowChrome(preferredPanelOwner);
}

bool SpikeScopeDockManager::sideOccupied(SpikeSortingDialog* window, DockSide side) const
{
    if (!window) return false;

    for (const DockLink& link : links) {
        if (link.first == window && link.secondSideOfFirst == side) return true;
        if (link.second == window && oppositeSide(link.secondSideOfFirst) == side) return true;
    }
    return false;
}

bool SpikeScopeDockManager::componentsWouldOverlap(
        const QList<SpikeSortingDialog*>& movingComponent,
        SpikeSortingDialog* targetWindow, const QPoint& translation) const
{
    const QList<SpikeSortingDialog*> targetComponent = connectedComponent(targetWindow);
    for (SpikeSortingDialog* movingWindow : movingComponent) {
        if (!movingWindow) continue;
        const QRect movedFrame = movingWindow->frameGeometry().translated(translation);
        for (SpikeSortingDialog* existingWindow : targetComponent) {
            if (existingWindow && movedFrame.intersects(existingWindow->frameGeometry())) return true;
        }
    }
    return false;
}

bool SpikeScopeDockManager::shouldReleaseSnapSuppression(SpikeSortingDialog* window) const
{
    if (!window) return true;

    const QRect frame = window->frameGeometry();
    const int releaseDistance = snapDistance * 2;
    for (const auto& otherPointer : windows) {
        SpikeSortingDialog* other = otherPointer;
        if (!other || other == window || !other->isVisible()) continue;

        const QRect otherFrame = other->frameGeometry();
        const int verticalOverlap = rangeOverlap(frame.top(), frame.bottom(), otherFrame.top(), otherFrame.bottom());
        const int horizontalOverlap = rangeOverlap(frame.left(), frame.right(), otherFrame.left(), otherFrame.right());
        if (verticalOverlap > 0 &&
                (qAbs(frame.left() - otherFrame.right() - 1) <= releaseDistance ||
                 qAbs(frame.right() - otherFrame.left() + 1) <= releaseDistance)) {
            return false;
        }
        if (horizontalOverlap > 0 &&
                (qAbs(frame.top() - otherFrame.bottom() - 1) <= releaseDistance ||
                 qAbs(frame.bottom() - otherFrame.top() + 1) <= releaseDistance)) {
            return false;
        }
    }

    return true;
}

SpikeScopeDockManager::DockSide SpikeScopeDockManager::oppositeSide(DockSide side)
{
    switch (side) {
    case DockLeft: return DockRight;
    case DockRight: return DockLeft;
    case DockTop: return DockBottom;
    case DockBottom: return DockTop;
    }
    return DockRight;
}

int SpikeScopeDockManager::rangeOverlap(int firstStart, int firstEnd, int secondStart, int secondEnd)
{
    return qMax(0, qMin(firstEnd, secondEnd) - qMax(firstStart, secondStart) + 1);
}

QPoint SpikeScopeDockManager::translationForSnap(const QRect& movingFrame, const QRect& targetFrame, DockSide side)
{
    QPoint desiredTopLeft = movingFrame.topLeft();
    switch (side) {
    case DockLeft:
        desiredTopLeft = QPoint(targetFrame.left() - movingFrame.width(), targetFrame.top());
        break;
    case DockRight:
        desiredTopLeft = QPoint(targetFrame.right() + 1, targetFrame.top());
        break;
    case DockTop:
        desiredTopLeft = QPoint(targetFrame.left(), targetFrame.top() - movingFrame.height());
        break;
    case DockBottom:
        desiredTopLeft = QPoint(targetFrame.left(), targetFrame.bottom() + 1);
        break;
    }
    return desiredTopLeft - movingFrame.topLeft();
}
