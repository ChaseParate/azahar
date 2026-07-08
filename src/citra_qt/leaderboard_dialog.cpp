#include "leaderboard_dialog.h"

#include <ctime>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/core.h"
#include "core/retroachievements/client.h"

// Mirrors RC_CLIENT_LEADERBOARD_STATE_* values from rc_client.h
static QString LbStateString(uint8_t state) {
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
    resize(800, 550);

    // ── Top: leaderboard list ────────────────────────────────────────────────
    lb_table = new QTableWidget(this);
    lb_table->setColumnCount(4);
    lb_table->setHorizontalHeaderLabels({tr("Title"), tr("Description"), tr("Current"), tr("State")});
    lb_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lb_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    lb_table->setSelectionMode(QAbstractItemView::SingleSelection);
    lb_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    lb_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    lb_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    lb_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    lb_table->verticalHeader()->setVisible(false);

    // ── Bottom: rank entry list ──────────────────────────────────────────────
    entries_header = new QLabel(tr("Select a leaderboard above to see top scores."), this);
    entries_header->setAlignment(Qt::AlignCenter);

    entry_table = new QTableWidget(this);
    entry_table->setColumnCount(4);
    entry_table->setHorizontalHeaderLabels({tr("Rank"), tr("User"), tr("Score"), tr("Date")});
    entry_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entry_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    entry_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    entry_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    entry_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    entry_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    entry_table->verticalHeader()->setVisible(false);

    auto* bottom_widget = new QWidget(this);
    auto* bottom_layout = new QVBoxLayout(bottom_widget);
    bottom_layout->setContentsMargins(0, 0, 0, 0);
    bottom_layout->addWidget(entries_header);
    bottom_layout->addWidget(entry_table);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(lb_table);
    splitter->addWidget(bottom_widget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(splitter);
    setLayout(layout);

    connect(lb_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        const auto selected = lb_table->selectedItems();
        if (!selected.isEmpty()) {
            OnLeaderboardSelected(lb_table->currentRow());
        }
    });

    PopulateLeaderboards();
}

void LeaderboardDialog::PopulateLeaderboards() {
    auto& client = system.RetroAchievementsClient();
    if (!client.IsGameLoaded()) {
        entries_header->setText(tr("No game loaded."));
        return;
    }

    const auto entries = client.GetLeaderboards();
    lb_table->setRowCount(static_cast<int>(entries.size()));

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const auto& e = entries[i];
        // Store leaderboard ID in user data of the first column item for fetch
        auto* title_item = new QTableWidgetItem(QString::fromStdString(e.title));
        title_item->setData(Qt::UserRole, static_cast<uint>(e.id));
        lb_table->setItem(i, 0, title_item);
        lb_table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(e.description)));
        lb_table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(e.tracker_value)));
        lb_table->setItem(i, 3, new QTableWidgetItem(LbStateString(e.state)));
    }
}

void LeaderboardDialog::OnLeaderboardSelected(int row) {
    auto* title_item = lb_table->item(row, 0);
    if (!title_item) return;

    const uint32_t lb_id = title_item->data(Qt::UserRole).toUInt();
    const QString lb_title = title_item->text();

    entries_header->setText(tr("Loading top scores for: %1...").arg(lb_title));
    entry_table->setRowCount(0);

    system.RetroAchievementsClient().FetchLeaderboardEntries(
        lb_id, 10,
        [this, lb_title](bool success, std::vector<RetroAchievements::LeaderboardRankEntry> entries) {
            // This fires on the HTTP thread — marshal to the UI thread.
            QMetaObject::invokeMethod(this, [this, lb_title, success,
                                             entries = std::move(entries)]() mutable {
                if (!success || entries.empty()) {
                    entries_header->setText(
                        tr("No scores found for: %1").arg(lb_title));
                    entry_table->setRowCount(0);
                    return;
                }

                entries_header->setText(
                    tr("Top scores for: %1").arg(lb_title));
                entry_table->setRowCount(static_cast<int>(entries.size()));

                for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                    const auto& e = entries[i];

                    auto* rank_item = new QTableWidgetItem();
                    rank_item->setData(Qt::DisplayRole, static_cast<int>(e.rank));
                    entry_table->setItem(i, 0, rank_item);

                    entry_table->setItem(
                        i, 1, new QTableWidgetItem(QString::fromStdString(e.user)));
                    entry_table->setItem(
                        i, 2, new QTableWidgetItem(QString::fromStdString(e.display)));

                    QString date_str;
                    if (e.submitted != 0) {
                        char buf[32];
                        struct tm t{};
#ifdef _WIN32
                        localtime_s(&t, &e.submitted);
#else
                        localtime_r(&e.submitted, &t);
#endif
                        strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
                        date_str = QString::fromLatin1(buf);
                    }
                    entry_table->setItem(i, 3, new QTableWidgetItem(date_str));
                }
            });
        });
}
