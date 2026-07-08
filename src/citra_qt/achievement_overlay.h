// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QLabel;
class QPropertyAnimation;
class QTimer;

/// Frameless tool-window overlay shown in the bottom-right corner of the render window
/// when a RetroAchievements achievement is unlocked. Auto-dismisses after 5 seconds.
/// Implemented as a Qt::Tool window (not a child widget) so it renders above the OpenGL
/// surface regardless of native-window Z-order.
class AchievementOverlay : public QWidget {
    Q_OBJECT

public:
    /// render_window — the GRenderWindow widget; used for positioning.
    explicit AchievementOverlay(QWidget* render_window);

    void ShowAchievement(const QString& title, const QString& description);

    /// Recalculate position relative to the current render window geometry.
    void Reposition();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void FadeOut();

    QWidget* render_window_;
    QLabel* title_label;
    QLabel* description_label;
    QTimer* dismiss_timer;
    QPropertyAnimation* fade_animation;
};
