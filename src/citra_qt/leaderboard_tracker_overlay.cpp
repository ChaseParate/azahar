// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/leaderboard_tracker_overlay.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

static constexpr int kMargin = 12;

LeaderboardTrackerOverlay::LeaderboardTrackerOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::SubWindow);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);

    label = new QLabel(this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color: #FFFFFF; font-weight: bold; font-size: 13px;"));
    layout->addWidget(label);
    setLayout(layout);

    hide();
}

void LeaderboardTrackerOverlay::Show(const QString& value) {
    if (value.isEmpty()) {
        hide();
        return;
    }
    label->setText(value);
    adjustSize();
    Reposition();
    show();
    raise();
}

void LeaderboardTrackerOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.setPen(QColor(80, 180, 255, 220));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);
}

void LeaderboardTrackerOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    Reposition();
}

void LeaderboardTrackerOverlay::Reposition() {
    if (!parentWidget()) return;
    const QSize parent_size = parentWidget()->size();
    // Top-right corner, opposite corner from the achievement overlay
    move(parent_size.width() - width() - kMargin, kMargin);
}
