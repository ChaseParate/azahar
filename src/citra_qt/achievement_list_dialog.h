#pragma once

#include <QDialog>

class QLabel;
class QTableWidget;

namespace Core {
class System;
}

class AchievementListDialog : public QDialog {
    Q_OBJECT

public:
    explicit AchievementListDialog(Core::System& system, QWidget* parent = nullptr);

    // Call after an achievement unlocks so the dialog refreshes without reopening.
    void Refresh();

private:
    void Populate();

    Core::System& system;
    QLabel* summary_label;
    QTableWidget* table;
};
