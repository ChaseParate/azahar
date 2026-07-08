#include "achievement_list_dialog.h"

#include <ctime>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/core.h"
#include "core/retroachievements/client.h"

// Mirrors RC_CLIENT_ACHIEVEMENT_BUCKET_* values from rc_client.h
static QString BucketLabel(uint8_t bucket) {
    switch (bucket) {
    case 1: return QStringLiteral("Locked");
    case 2: return QStringLiteral("Unlocked");
    case 3: return QStringLiteral("Unsupported");
    case 4: return QStringLiteral("Unofficial");
    case 5: return QStringLiteral("Recently Unlocked");
    case 6: return QStringLiteral("Active Challenge");
    case 7: return QStringLiteral("Almost There");
    case 8: return QStringLiteral("Unsynced");
    default: return QStringLiteral("Unknown");
    }
}

AchievementListDialog::AchievementListDialog(Core::System& system_, QWidget* parent)
    : QDialog(parent), system(system_) {
    setWindowTitle(tr("RetroAchievements"));
    resize(800, 500);

    summary_label = new QLabel(this);
    summary_label->setAlignment(Qt::AlignCenter);

    table = new QTableWidget(this);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(
        {tr("Title"), tr("Description"), tr("Pts"), tr("Progress"), tr("Status"), tr("Unlocked")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSortingEnabled(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary_label);
    layout->addWidget(table);
    setLayout(layout);

    Populate();
}

void AchievementListDialog::Refresh() {
    Populate();
}

void AchievementListDialog::Populate() {
    auto& client = system.RetroAchievementsClient();
    if (!client.IsGameLoaded()) {
        summary_label->setText(tr("No game loaded."));
        table->setRowCount(0);
        return;
    }

    const auto entries = client.GetAchievements();

    uint32_t unlocked = 0;
    uint32_t total_pts = 0;
    uint32_t earned_pts = 0;
    for (const auto& e : entries) {
        total_pts += e.points;
        // state == 2 means RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED
        if (e.state == 2) {
            ++unlocked;
            earned_pts += e.points;
        }
    }

    summary_label->setText(
        tr("%1 / %2 achievements unlocked  (%3 / %4 points)")
            .arg(unlocked)
            .arg(entries.size())
            .arg(earned_pts)
            .arg(total_pts));

    table->setSortingEnabled(false);
    table->setRowCount(static_cast<int>(entries.size()));

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const auto& e = entries[i];

        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(e.title)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(e.description)));

        auto* pts_item = new QTableWidgetItem();
        pts_item->setData(Qt::DisplayRole, static_cast<int>(e.points));
        table->setItem(i, 2, pts_item);

        table->setItem(i, 3,
                       new QTableWidgetItem(QString::fromStdString(e.measured_progress)));
        table->setItem(i, 4, new QTableWidgetItem(BucketLabel(e.bucket)));

        QString unlock_str;
        if (e.unlock_time != 0) {
            char buf[32];
            struct tm t{};
#ifdef _WIN32
            localtime_s(&t, &e.unlock_time);
#else
            localtime_r(&e.unlock_time, &t);
#endif
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &t);
            unlock_str = QString::fromLatin1(buf);
        }
        table->setItem(i, 5, new QTableWidgetItem(unlock_str));
    }

    table->setSortingEnabled(true);
    table->sortByColumn(4, Qt::AscendingOrder); // group by status (locked first)
}
