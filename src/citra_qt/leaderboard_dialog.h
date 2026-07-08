#pragma once

#include <QDialog>

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

    Core::System& system;
    QTableWidget* table;
};
