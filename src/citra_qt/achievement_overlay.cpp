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

static constexpr int kDisplayMs   = 5000; // fully visible duration
static constexpr int kFadeMs      = 1000; // fade-out duration
static constexpr int kMargin      = 16;   // px from window edge
static constexpr int kPanelWidth  = 320;
static constexpr int kPanelRadius = 8;

AchievementOverlay::AchievementOverlay(QWidget* parent) : QWidget(parent) {
    // Don't steal focus or intercept mouse events from the game
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::SubWindow);

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
    // Stop any in-progress fade so the new achievement starts fully visible
    fade_animation->stop();
    auto* opacity = qobject_cast<QGraphicsOpacityEffect*>(graphicsEffect());
    if (opacity) opacity->setOpacity(1.0);

    title_label->setText(QStringLiteral("🏆 %1").arg(title));
    description_label->setText(description);

    adjustSize();
    Reposition();
    show();
    raise();

    dismiss_timer->start(kDisplayMs);
}

void AchievementOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(20, 20, 20, 210));
    painter.setPen(QColor(180, 140, 0, 200));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), kPanelRadius, kPanelRadius);
}

void AchievementOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    Reposition();
}

void AchievementOverlay::Reposition() {
    if (!parentWidget()) return;
    const QSize parent_size = parentWidget()->size();
    move(parent_size.width()  - width()  - kMargin,
         parent_size.height() - height() - kMargin);
}

void AchievementOverlay::FadeOut() {
    fade_animation->start();
}
