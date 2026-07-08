#pragma once

#include <QDialog>

class QLabel;
class QTableWidget;

namespace Core {
class System;
}

class LeaderboardDialog : public QDialog {
    Q_OBJECT

public:
    explicit LeaderboardDialog(Core::System& system, QWidget* parent = nullptr);

private:
    void PopulateLeaderboards();
    void OnLeaderboardSelected(int row);

    Core::System& system;
    QTableWidget* lb_table;    // list of leaderboards for this game
    QLabel* entries_header;    // "Top scores for: <title>"
    QTableWidget* entry_table; // fetched rank entries
};
