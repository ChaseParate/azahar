// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QLabel;
class QPropertyAnimation;
class QTimer;

/// Non-blocking overlay shown in the bottom-right corner of the render window
/// when a RetroAchievements achievement is unlocked. Auto-dismisses after 6 s.
class AchievementOverlay : public QWidget {
    Q_OBJECT

public:
    explicit AchievementOverlay(QWidget* parent = nullptr);

    /// Show the overlay for the given achievement. Safe to call from any thread
    /// via QMetaObject::invokeMethod with Qt::QueuedConnection.
    void ShowAchievement(const QString& title, const QString& description);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void Reposition();
    void FadeOut();

    QLabel* title_label;
    QLabel* description_label;
    QTimer* dismiss_timer;
    QPropertyAnimation* fade_animation;
};
