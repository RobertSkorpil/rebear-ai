#pragma once

#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>
#include "rebear/transaction.h"

namespace rebear {
namespace gui {

/**
 * @brief Model for displaying transactions in a QTableView
 */
class TransactionModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit TransactionModel(QObject* parent = nullptr);

    // Add a new transaction
    void addTransaction(const rebear::Transaction& trans);

    // Clear all transactions
    void clear();

    // Get transaction at row
    const rebear::Transaction* getTransaction(int row) const;

    // Load flash data for displaying actual bytes
    bool loadFlashData(const std::string& filename);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    std::vector<rebear::Transaction> transactions_;
    std::vector<uint8_t> flashData_;
    
    // Column indices
    enum Column {
        COL_TIMESTAMP = 0,
        COL_ADDRESS,
        COL_COUNT,
        COL_DATA,
        COL_COUNT_MAX
    };
};

/**
 * @brief Widget for viewing and managing transaction history
 */
class TransactionViewer : public QWidget {
    Q_OBJECT

public:
    explicit TransactionViewer(QWidget* parent = nullptr);

    // Add a transaction to the view
    void addTransaction(const rebear::Transaction& trans);

    // Clear all transactions
    void clear();

    // Get number of transactions
    int transactionCount() const;

    // Load flash data for displaying actual bytes
    bool loadFlashData(const std::string& filename);

signals:
    // Emitted when user clicks on a transaction
    void transactionClicked(uint32_t address);

    // Emitted when user requests export
    void exportRequested();

public slots:
    // Enable/disable auto-scroll
    void setAutoScroll(bool enabled);

private slots:
    void onClearClicked();
    void onExportClicked();
    void onSearchTextChanged(const QString& text);
    void onTableClicked(const QModelIndex& index);

private:
    void setupUi();
    void applySearchFilter();

    // Widgets
    QTableView* tableView_;
    TransactionModel* model_;
    QPushButton* btnClear_;
    QPushButton* btnExport_;
    QCheckBox* chkAutoScroll_;
    QLineEdit* editSearch_;

    // State
    bool autoScroll_;
};

} // namespace gui
} // namespace rebear
