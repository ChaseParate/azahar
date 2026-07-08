// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/achievement_overlay.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

static constexpr int kDisplayMs   = 5000;
static constexpr int kFadeMs      = 1000;
static constexpr int kMargin      = 16;
static constexpr int kPanelWidth  = 320;
static constexpr int kPanelRadius = 8;

AchievementOverlay::AchievementOverlay(QWidget* render_window)
    // Qt::Tool window parented to the top-level main window so it:
    //   • stays associated with (and in front of) the main window
    //   • is automatically destroyed when the main window closes
    //   • renders above the OpenGL native surface (separate OS-level window)
    : QWidget(render_window ? render_window->window() : nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus),
      render_window_(render_window) {

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    title_label = new QLabel(this);
    title_label->setWordWrap(true);
    title_label->setStyleSheet(
        QStringLiteral("color: #FFD700; font-weight: bold; font-size: 13px;"));

    description_label = new QLabel(this);
    description_label->setWordWrap(true);
    description_label->setStyleSheet(
        QStringLiteral("color: #FFFFFF; font-size: 11px;"));

    layout->addWidget(title_label);
    layout->addWidget(description_label);
    setLayout(layout);
    setFixedWidth(kPanelWidth);

    auto* opacity = new QGraphicsOpacityEffect(this);
    opacity->setOpacity(1.0);
    setGraphicsEffect(opacity);

    fade_animation = new QPropertyAnimation(opacity, "opacity", this);
    fade_animation->setDuration(kFadeMs);
    fade_animation->setStartValue(1.0);
    fade_animation->setEndValue(0.0);
    connect(fade_animation, &QPropertyAnimation::finished, this, &QWidget::hide);

    dismiss_timer = new QTimer(this);
    dismiss_timer->setSingleShot(true);
    connect(dismiss_timer, &QTimer::timeout, this, &AchievementOverlay::FadeOut);

    hide();
}

void AchievementOverlay::ShowAchievement(const QString& title, const QString& description) {
    fade_animation->stop();
    auto* opacity = qobject_cast<QGraphicsOpacityEffect*>(graphicsEffect());
    if (opacity) opacity->setOpacity(1.0);

    title_label->setText(QStringLiteral("\U0001F3C6 %1").arg(title));
    description_label->setText(description);

    adjustSize();
    Reposition();
    show();
    raise();

    dismiss_timer->start(kDisplayMs);
}

void AchievementOverlay::Reposition() {
    if (!render_window_) return;
    // Map render window's bottom-right corner to screen coordinates, then
    // subtract our size and margin to anchor in the bottom-right of the game view.
    const QPoint br = render_window_->mapToGlobal(render_window_->rect().bottomRight());
    move(br.x() - width() - kMargin, br.y() - height() - kMargin);
}

void AchievementOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(20, 20, 20, 210));
    painter.setPen(QColor(180, 140, 0, 200));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), kPanelRadius, kPanelRadius);
}

void AchievementOverlay::FadeOut() {
    fade_animation->start();
}
