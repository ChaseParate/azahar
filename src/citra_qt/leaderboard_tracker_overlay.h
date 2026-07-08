// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QLabel;

/// Frameless tool-window HUD shown in the top-right corner of the render window while a
/// leaderboard attempt is in progress. Call Show("") to hide.
/// Implemented as a Qt::Tool window so it renders above the OpenGL surface.
class LeaderboardTrackerOverlay : public QWidget {
    Q_OBJECT

public:
    explicit LeaderboardTrackerOverlay(QWidget* render_window);

    void Show(const QString& value);

    /// Recalculate position relative to the current render window geometry.
    void Reposition();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QWidget* render_window_;
    QLabel* label;
};
