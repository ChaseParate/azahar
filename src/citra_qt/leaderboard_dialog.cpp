#include "leaderboard_dialog.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/core.h"
#include "core/retroachievements/client.h"

// Values mirror RC_CLIENT_LEADERBOARD_STATE_* in rc_client.h
static QString StateToString(uint8_t state) {
    switch (state) {
    case 0: return QStringLiteral("Inactive");
    case 1: return QStringLiteral("Active");
    case 2: return QStringLiteral("Tracking");
    case 3: return QStringLiteral("Disabled");
    default: return QStringLiteral("Unknown");
    }
}

LeaderboardDialog::LeaderboardDialog(Core::System& system_, QWidget* parent)
    : QDialog(parent), system(system_) {
    setWindowTitle(tr("RetroAchievements Leaderboards"));
    resize(700, 450);

    table = new QTableWidget(this);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({tr("Title"), tr("Description"), tr("Current"), tr("State")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table);
    setLayout(layout);

    PopulateLeaderboards();
}

void LeaderboardDialog::PopulateLeaderboards() {
    auto& client = system.RetroAchievementsClient();
    if (!client.IsGameLoaded()) {
        auto* label = new QLabel(tr("No game loaded."), this);
        layout()->addWidget(label);
        return;
    }

    auto entries = client.GetLeaderboards();
    table->setRowCount(static_cast<int>(entries.size()));

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const auto& e = entries[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(e.title)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(e.description)));
        table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(e.tracker_value)));
        table->setItem(i, 3, new QTableWidgetItem(StateToString(e.state)));
    }
}
