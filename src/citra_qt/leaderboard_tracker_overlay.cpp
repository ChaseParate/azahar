// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/leaderboard_tracker_overlay.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

static constexpr int kMargin = 12;

LeaderboardTrackerOverlay::LeaderboardTrackerOverlay(QWidget* render_window)
    : QWidget(render_window ? render_window->window() : nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus),
      render_window_(render_window) {

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

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

void LeaderboardTrackerOverlay::Reposition() {
    if (!render_window_) return;
    // Top-right corner — opposite corner from the achievement overlay.
    const QPoint tr = render_window_->mapToGlobal(render_window_->rect().topRight());
    move(tr.x() - width() - kMargin, tr.y() + kMargin);
}
