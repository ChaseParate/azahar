#pragma once

#include <QWidget>

class QLabel;

// A persistent HUD label shown in the top-right corner of the render window
// while a leaderboard attempt is in progress. Call Show("") to hide.
class LeaderboardTrackerOverlay : public QWidget {
    Q_OBJECT

public:
    explicit LeaderboardTrackerOverlay(QWidget* parent);

    void Show(const QString& value);
    void Reposition();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel* label;
};
