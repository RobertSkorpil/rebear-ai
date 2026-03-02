#include "transaction_viewer.h"
#include <QHeaderView>
#include <QLabel>
#include <QGroupBox>
#include <QScrollBar>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace rebear {
namespace gui {

// ============================================================================
// TransactionModel Implementation
// ============================================================================

TransactionModel::TransactionModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void TransactionModel::addTransaction(const rebear::Transaction& trans) {
    int row = transactions_.size();
    beginInsertRows(QModelIndex(), row, row);
    transactions_.push_back(trans);
    endInsertRows();
}

void TransactionModel::clear() {
    beginResetModel();
    transactions_.clear();
    endResetModel();
}

const rebear::Transaction* TransactionModel::getTransaction(int row) const {
    if (row >= 0 && row < static_cast<int>(transactions_.size())) {
        return &transactions_[row];
    }
    return nullptr;
}

bool TransactionModel::loadFlashData(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    flashData_.resize(size);
    file.read(reinterpret_cast<char*>(flashData_.data()), size);
    
    return true;
}

int TransactionModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return transactions_.size();
}

int TransactionModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return COL_COUNT_MAX;
}

QVariant TransactionModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(transactions_.size())) {
        return QVariant();
    }

    const auto& trans = transactions_[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case COL_TIMESTAMP: {
                std::ostringstream oss;
                oss << trans.timestamp << " ms";
                return QString::fromStdString(oss.str());
            }
            case COL_ADDRESS: {
                std::ostringstream oss;
                oss << "0x" << std::hex << std::setw(6) << std::setfill('0') 
                    << std::uppercase << trans.address;
                return QString::fromStdString(oss.str());
            }
            case COL_COUNT: {
                if (trans.count == 0xFFFFFF) {
                    return QString("PATCHED");
                }
                std::ostringstream oss;
                oss << trans.count << " bytes";
                return QString::fromStdString(oss.str());
            }
            case COL_DATA: {
                // Show first 16 bytes of actual data from flash.bin
                if (flashData_.empty() || trans.address >= flashData_.size()) {
                    return QString("(no data)");
                }
                
                std::ostringstream oss;
                size_t bytesToShow = std::min(static_cast<size_t>(16),
                                             flashData_.size() - trans.address);
                bytesToShow = std::min(bytesToShow, static_cast<size_t>(trans.count));
                
                for (size_t i = 0; i < bytesToShow; ++i) {
                    if (i > 0) oss << " ";
                    oss << std::hex << std::setw(2) << std::setfill('0')
                        << std::uppercase << static_cast<int>(flashData_[trans.address + i]);
                }
                
                if (trans.count > 16) {
                    oss << "...";
                }
                
                return QString::fromStdString(oss.str());
            }
        }
    } else if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    } else if (role == Qt::BackgroundRole) {
        // Color code by address range
        uint32_t addr = trans.address;
        if (addr < 0x010000) {
            return QColor(255, 240, 240);  // Light red - low addresses
        } else if (addr < 0x100000) {
            return QColor(240, 255, 240);  // Light green - mid addresses
        } else {
            return QColor(240, 240, 255);  // Light blue - high addresses
        }
    } else if (role == Qt::ForegroundRole) {
        // Highlight patched transactions
        if (trans.count == 0xFFFFFF) {
            return QColor(255, 0, 0);  // Red text for patched
        }
    }

    return QVariant();
}

QVariant TransactionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case COL_TIMESTAMP:
                return QString("Timestamp");
            case COL_ADDRESS:
                return QString("Address");
            case COL_COUNT:
                return QString("Count");
            case COL_DATA:
                return QString("Data (first 16 bytes)");
        }
    } else {
        return QString::number(section + 1);
    }

    return QVariant();
}

// ============================================================================
// TransactionViewer Implementation
// ============================================================================

TransactionViewer::TransactionViewer(QWidget* parent)
    : QWidget(parent)
    , autoScroll_(true)
{
    setupUi();
}

void TransactionViewer::setupUi() {
    // Create model and view
    model_ = new TransactionModel(this);
    tableView_ = new QTableView(this);
    tableView_->setModel(model_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->setSortingEnabled(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);

    // Connect click signal
    connect(tableView_, &QTableView::clicked, this, &TransactionViewer::onTableClicked);

    // Create control buttons
    btnClear_ = new QPushButton("Clear", this);
    btnExport_ = new QPushButton("Export", this);
    chkAutoScroll_ = new QCheckBox("Auto-scroll", this);
    chkAutoScroll_->setChecked(true);

    connect(btnClear_, &QPushButton::clicked, this, &TransactionViewer::onClearClicked);
    connect(btnExport_, &QPushButton::clicked, this, &TransactionViewer::onExportClicked);
    connect(chkAutoScroll_, &QCheckBox::toggled, this, &TransactionViewer::setAutoScroll);

    // Create search box
    editSearch_ = new QLineEdit(this);
    editSearch_->setPlaceholderText("Search address (hex)...");
    connect(editSearch_, &QLineEdit::textChanged, this, &TransactionViewer::onSearchTextChanged);

    // Layout controls
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel("Search:", this));
    controlLayout->addWidget(editSearch_);
    controlLayout->addStretch();
    controlLayout->addWidget(chkAutoScroll_);
    controlLayout->addWidget(btnClear_);
    controlLayout->addWidget(btnExport_);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(tableView_);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);
}

void TransactionViewer::addTransaction(const rebear::Transaction& trans) {
    model_->addTransaction(trans);

    // Auto-scroll to bottom if enabled
    if (autoScroll_) {
        tableView_->scrollToBottom();
    }
}

void TransactionViewer::clear() {
    model_->clear();
}

int TransactionViewer::transactionCount() const {
    return model_->rowCount();
}

void TransactionViewer::setAutoScroll(bool enabled) {
    autoScroll_ = enabled;
}

bool TransactionViewer::loadFlashData(const std::string& filename) {
    return model_->loadFlashData(filename);
}

void TransactionViewer::onClearClicked() {
    clear();
}

void TransactionViewer::onExportClicked() {
    emit exportRequested();
}

void TransactionViewer::onSearchTextChanged(const QString& text) {
    // Simple search implementation - could be enhanced with proxy model
    if (text.isEmpty()) {
        // Show all rows
        for (int i = 0; i < model_->rowCount(); ++i) {
            tableView_->setRowHidden(i, false);
        }
        return;
    }

    // Parse search text as hex address
    bool ok;
    uint32_t searchAddr = text.toUInt(&ok, 16);
    if (!ok) {
        return;
    }

    // Hide rows that don't match
    for (int i = 0; i < model_->rowCount(); ++i) {
        const auto* trans = model_->getTransaction(i);
        if (trans) {
            bool matches = (trans->address == searchAddr);
            tableView_->setRowHidden(i, !matches);
        }
    }
}

void TransactionViewer::onTableClicked(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }

    const auto* trans = model_->getTransaction(index.row());
    if (trans) {
        emit transactionClicked(trans->address);
    }
}

} // namespace gui
} // namespace rebear
