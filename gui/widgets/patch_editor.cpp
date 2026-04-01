#include "patch_editor.h"
#include <QHeaderView>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <iomanip>
#include <sstream>

namespace rebear {
namespace gui {

// ============================================================================
// PatchModel Implementation
// ============================================================================

PatchModel::PatchModel(QObject* parent)
    : QAbstractTableModel(parent)
    , patches_(nullptr)
{
}

void PatchModel::setPatches(std::vector<rebear::Patch>* patches) {
    patches_ = patches;
    refresh();
}

void PatchModel::refresh() {
    beginResetModel();
    patchCache_.clear();
    if (patches_) {
        patchCache_ = *patches_;
    }
    endResetModel();
}

const rebear::Patch* PatchModel::getPatch(int row) const {
    if (row >= 0 && row < static_cast<int>(patchCache_.size())) {
        return &patchCache_[row];
    }
    return nullptr;
}

int PatchModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return patchCache_.size();
}

int PatchModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return COL_COUNT_MAX;
}

QVariant PatchModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(patchCache_.size())) {
        return QVariant();
    }

    const auto& patch = patchCache_[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case COL_ID:
                return QString::number(patch.id);
            case COL_ADDRESS: {
                std::ostringstream oss;
                oss << "0x" << std::hex << std::setw(6) << std::setfill('0') 
                    << std::uppercase << patch.address;
                return QString::fromStdString(oss.str());
            }
            case COL_DATA: {
                std::ostringstream oss;
                for (size_t i = 0; i < patch.data.size(); ++i) {
                    if (i > 0) oss << " ";
                    oss << std::hex << std::setw(2) << std::setfill('0') 
                        << std::uppercase << static_cast<int>(patch.data[i]);
                }
                return QString::fromStdString(oss.str());
            }
            case COL_ENABLED:
                return patch.enabled ? QString("Active") : QString("Disabled");
        }
    } else if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    } else if (role == Qt::BackgroundRole) {
        if (!patch.enabled) {
            return QColor(240, 240, 240);  // Gray for disabled
        } else {
            return QColor(240, 255, 240);  // Light green for active
        }
    }

    return QVariant();
}

QVariant PatchModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case COL_ID:
                return QString("ID");
            case COL_ADDRESS:
                return QString("Address");
            case COL_DATA:
                return QString("Data");
            case COL_ENABLED:
                return QString("Status");
        }
    } else {
        return QString::number(section + 1);
    }

    return QVariant();
}

// ============================================================================
// PatchDialog Implementation
// ============================================================================

PatchDialog::PatchDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void PatchDialog::setupUi() {
    setWindowTitle("Add/Edit Patch");
    setModal(true);

    // Create widgets
    spinId_ = new QSpinBox(this);
    spinId_->setRange(0, 15);
    spinId_->setValue(0);

    editAddress_ = new QLineEdit(this);
    editAddress_->setPlaceholderText("e.g., 001000");
    editAddress_->setMaxLength(6);

    editData_ = new QLineEdit(this);
    editData_->setPlaceholderText("e.g., 0102030405060708 (variable length, max 512 bytes)");
    editData_->setMaxLength(1024);  // 512 bytes = 1024 hex characters
    connect(editData_, &QLineEdit::textChanged, this, &PatchDialog::onDataTextChanged);

    lblDataPreview_ = new QLabel(this);
    lblDataPreview_->setWordWrap(true);
    lblDataPreview_->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; }");

    btnOk_ = new QPushButton("OK", this);
    btnCancel_ = new QPushButton("Cancel", this);
    connect(btnOk_, &QPushButton::clicked, this, &PatchDialog::validateAndAccept);
    connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);

    // Layout
    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow("Patch ID (0-15):", spinId_);
    formLayout->addRow("Address (hex):", editAddress_);
    formLayout->addRow("Data (16 hex chars):", editData_);
    formLayout->addRow("Preview:", lblDataPreview_);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnOk_);
    buttonLayout->addWidget(btnCancel_);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void PatchDialog::setPatch(const rebear::Patch& patch) {
    spinId_->setValue(patch.id);

    std::ostringstream oss;
    oss << std::hex << std::setw(6) << std::setfill('0') << patch.address;
    editAddress_->setText(QString::fromStdString(oss.str()));

    oss.str("");
    for (const auto& byte : patch.data) {
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(byte);
    }
    editData_->setText(QString::fromStdString(oss.str()));
}

rebear::Patch PatchDialog::getPatch() const {
    rebear::Patch patch;
    patch.id = spinId_->value();

    // Parse address
    bool ok;
    patch.address = editAddress_->text().toUInt(&ok, 16);

    // Parse data - variable length
    QString dataStr = editData_->text();
    patch.data.clear();
    
    // Parse all hex byte pairs
    for (int i = 0; i * 2 < dataStr.length(); ++i) {
        if (i * 2 + 1 < dataStr.length()) {
            QString byteStr = dataStr.mid(i * 2, 2);
            patch.data.push_back(static_cast<uint8_t>(byteStr.toUInt(&ok, 16)));
        }
    }

    patch.enabled = true;
    return patch;
}

void PatchDialog::onDataTextChanged() {
    QString text = editData_->text().toUpper();
    
    // Remove non-hex characters
    QString cleaned;
    for (const QChar& c : text) {
        if (c.isDigit() || (c >= 'A' && c <= 'F')) {
            cleaned += c;
        }
    }
    
    if (cleaned != text) {
        editData_->setText(cleaned);
        return;
    }

    // Update preview
    if (cleaned.length() >= 2 && cleaned.length() % 2 == 0) {
        int numBytes = cleaned.length() / 2;
        QString preview = QString("%1 bytes: ").arg(numBytes);
        
        // Show first few bytes
        int showBytes = std::min(numBytes, 8);
        for (int i = 0; i < showBytes; ++i) {
            if (i > 0) preview += " ";
            preview += cleaned.mid(i * 2, 2);
        }
        if (numBytes > 8) {
            preview += QString(" ... (+%1 more)").arg(numBytes - 8);
        }
        
        lblDataPreview_->setText(preview);
        lblDataPreview_->setStyleSheet("QLabel { background-color: #d0ffd0; padding: 5px; }");
    } else {
        QString msg = cleaned.length() % 2 == 0 ? 
            QString("Need at least 2 hex characters (have %1)").arg(cleaned.length()) :
            QString("Need even number of hex characters (have %1)").arg(cleaned.length());
        lblDataPreview_->setText(msg);
        lblDataPreview_->setStyleSheet("QLabel { background-color: #ffd0d0; padding: 5px; }");
    }
}

bool PatchDialog::validateData() {
    // Validate address
    bool ok;
    uint32_t addr = editAddress_->text().toUInt(&ok, 16);
    if (!ok || addr >= 0x1000000) {
        QMessageBox::warning(this, "Invalid Address", 
            "Address must be a valid 24-bit hex value (000000-FFFFFF)");
        return false;
    }

    // Validate data
    QString dataStr = editData_->text();
    if (dataStr.length() < 2 || dataStr.length() % 2 != 0) {
        QMessageBox::warning(this, "Invalid Data", 
            "Data must be at least 2 hex characters and an even number of characters");
        return false;
    }
    
    if (dataStr.length() > 1024) {  // 512 bytes max
        QMessageBox::warning(this, "Invalid Data", 
            "Data exceeds maximum length of 512 bytes (1024 hex characters)");
        return false;
    }

    return true;
}

void PatchDialog::validateAndAccept() {
    if (validateData()) {
        accept();
    }
}

// ============================================================================
// PatchEditor Implementation
// ============================================================================

PatchEditor::PatchEditor(QWidget* parent)
    : QWidget(parent)
    , patches_(nullptr)
{
    setupUi();
}

void PatchEditor::setupUi() {
    // Create model and view
    model_ = new PatchModel(this);
    tableView_ = new QTableView(this);
    tableView_->setModel(model_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);

    // Connect double-click signal
    connect(tableView_, &QTableView::doubleClicked, this, &PatchEditor::onTableDoubleClicked);

    // Create buttons
    btnAdd_ = new QPushButton("Add", this);
    btnEdit_ = new QPushButton("Edit", this);
    btnRemove_ = new QPushButton("Remove", this);
    btnApplyAll_ = new QPushButton("Apply All", this);
    btnClearAll_ = new QPushButton("Clear All", this);

    connect(btnAdd_, &QPushButton::clicked, this, &PatchEditor::onAddClicked);
    connect(btnEdit_, &QPushButton::clicked, this, &PatchEditor::onEditClicked);
    connect(btnRemove_, &QPushButton::clicked, this, &PatchEditor::onRemoveClicked);
    connect(btnApplyAll_, &QPushButton::clicked, this, &PatchEditor::onApplyAllClicked);
    connect(btnClearAll_, &QPushButton::clicked, this, &PatchEditor::onClearAllClicked);

    // Layout buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(btnAdd_);
    buttonLayout->addWidget(btnEdit_);
    buttonLayout->addWidget(btnRemove_);
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnApplyAll_);
    buttonLayout->addWidget(btnClearAll_);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel("Patches:", this));
    mainLayout->addWidget(tableView_);
    mainLayout->addLayout(buttonLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);
}

void PatchEditor::setPatches(std::vector<rebear::Patch>* patches) {
    patches_ = patches;
    model_->setPatches(patches);
}

void PatchEditor::refresh() {
    model_->refresh();
}

void PatchEditor::onAddClicked() {
    if (!patches_) {
        return;
    }

    PatchDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        rebear::Patch patch = dialog.getPatch();
        if (patch.isValid()) {
            patches_->push_back(patch);
            refresh();
            emit patchesChanged();
        } else {
            QMessageBox::warning(this, "Error", "Invalid patch data");
        }
    }
}

void PatchEditor::onEditClicked() {
    if (!patches_) {
        return;
    }

    QModelIndex index = tableView_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "No Selection", "Please select a patch to edit");
        return;
    }

    const rebear::Patch* patch = model_->getPatch(index.row());
    if (!patch) {
        return;
    }

    PatchDialog dialog(this);
    dialog.setPatch(*patch);
    if (dialog.exec() == QDialog::Accepted) {
        rebear::Patch newPatch = dialog.getPatch();
        if (newPatch.isValid()) {
            // Find and replace in the actual vector
            for (auto& p : *patches_) {
                if (p.id == patch->id && p.address == patch->address) {
                    p = newPatch;
                    break;
                }
            }
            refresh();
            emit patchesChanged();
        } else {
            QMessageBox::warning(this, "Error", "Invalid patch data");
        }
    }
}

void PatchEditor::onRemoveClicked() {
    if (!patches_) {
        return;
    }

    QModelIndex index = tableView_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "No Selection", "Please select a patch to remove");
        return;
    }

    const rebear::Patch* patch = model_->getPatch(index.row());
    if (!patch) {
        return;
    }

    // Find and remove from the actual vector
    for (auto it = patches_->begin(); it != patches_->end(); ++it) {
        if (it->id == patch->id && it->address == patch->address) {
            patches_->erase(it);
            break;
        }
    }
    refresh();
    emit patchesChanged();
}

void PatchEditor::onApplyAllClicked() {
    emit applyAllRequested();
}

void PatchEditor::onClearAllClicked() {
    emit clearAllRequested();
}

void PatchEditor::onTableDoubleClicked(const QModelIndex& index) {
    if (index.isValid()) {
        onEditClicked();
    }
}

} // namespace gui
} // namespace rebear
