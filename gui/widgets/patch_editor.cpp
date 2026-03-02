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
    , patchManager_(nullptr)
{
}

void PatchModel::setPatchManager(rebear::PatchManager* manager) {
    patchManager_ = manager;
    refresh();
}

void PatchModel::refresh() {
    beginResetModel();
    patches_.clear();
    if (patchManager_) {
        patches_ = patchManager_->getPatches();
    }
    endResetModel();
}

const rebear::Patch* PatchModel::getPatch(int row) const {
    if (row >= 0 && row < static_cast<int>(patches_.size())) {
        return &patches_[row];
    }
    return nullptr;
}

int PatchModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return patches_.size();
}

int PatchModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return COL_COUNT_MAX;
}

QVariant PatchModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(patches_.size())) {
        return QVariant();
    }

    const auto& patch = patches_[index.row()];

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
                return QString("Data (8 bytes)");
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
    editData_->setPlaceholderText("e.g., 0102030405060708");
    editData_->setMaxLength(16);
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

    // Parse data
    QString dataStr = editData_->text();
    for (size_t i = 0; i < 8 && i * 2 < static_cast<size_t>(dataStr.length()); ++i) {
        QString byteStr = dataStr.mid(i * 2, 2);
        patch.data[i] = byteStr.toUInt(&ok, 16);
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
    if (cleaned.length() == 16) {
        QString preview = "Bytes: ";
        for (int i = 0; i < 8; ++i) {
            if (i > 0) preview += " ";
            preview += cleaned.mid(i * 2, 2);
        }
        lblDataPreview_->setText(preview);
        lblDataPreview_->setStyleSheet("QLabel { background-color: #d0ffd0; padding: 5px; }");
    } else {
        lblDataPreview_->setText(QString("Need 16 hex characters (have %1)").arg(cleaned.length()));
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
    if (dataStr.length() != 16) {
        QMessageBox::warning(this, "Invalid Data", 
            "Data must be exactly 16 hex characters (8 bytes)");
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
    , patchManager_(nullptr)
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
    btnLoad_ = new QPushButton("Load", this);
    btnSave_ = new QPushButton("Save", this);

    connect(btnAdd_, &QPushButton::clicked, this, &PatchEditor::onAddClicked);
    connect(btnEdit_, &QPushButton::clicked, this, &PatchEditor::onEditClicked);
    connect(btnRemove_, &QPushButton::clicked, this, &PatchEditor::onRemoveClicked);
    connect(btnApplyAll_, &QPushButton::clicked, this, &PatchEditor::onApplyAllClicked);
    connect(btnClearAll_, &QPushButton::clicked, this, &PatchEditor::onClearAllClicked);
    connect(btnLoad_, &QPushButton::clicked, this, &PatchEditor::onLoadClicked);
    connect(btnSave_, &QPushButton::clicked, this, &PatchEditor::onSaveClicked);

    // Layout buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(btnAdd_);
    buttonLayout->addWidget(btnEdit_);
    buttonLayout->addWidget(btnRemove_);
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnLoad_);
    buttonLayout->addWidget(btnSave_);
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

void PatchEditor::setPatchManager(rebear::PatchManager* manager) {
    patchManager_ = manager;
    model_->setPatchManager(manager);
}

void PatchEditor::refresh() {
    model_->refresh();
}

void PatchEditor::onAddClicked() {
    if (!patchManager_) {
        return;
    }

    PatchDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        rebear::Patch patch = dialog.getPatch();
        if (patchManager_->addPatch(patch)) {
            refresh();
            emit patchesChanged();
        } else {
            QMessageBox::warning(this, "Error", 
                QString::fromStdString(patchManager_->getLastError()));
        }
    }
}

void PatchEditor::onEditClicked() {
    if (!patchManager_) {
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
        // Remove old patch and add new one
        patchManager_->removePatch(patch->id);
        rebear::Patch newPatch = dialog.getPatch();
        if (patchManager_->addPatch(newPatch)) {
            refresh();
            emit patchesChanged();
        } else {
            QMessageBox::warning(this, "Error", 
                QString::fromStdString(patchManager_->getLastError()));
        }
    }
}

void PatchEditor::onRemoveClicked() {
    if (!patchManager_) {
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

    if (patchManager_->removePatch(patch->id)) {
        refresh();
        emit patchesChanged();
    } else {
        QMessageBox::warning(this, "Error", 
            QString::fromStdString(patchManager_->getLastError()));
    }
}

void PatchEditor::onApplyAllClicked() {
    emit applyAllRequested();
}

void PatchEditor::onClearAllClicked() {
    emit clearAllRequested();
}

void PatchEditor::onLoadClicked() {
    if (!patchManager_) {
        return;
    }

    QString filename = QFileDialog::getOpenFileName(this, "Load Patches", 
        "", "JSON Files (*.json);;All Files (*)");
    
    if (filename.isEmpty()) {
        return;
    }

    if (patchManager_->loadFromFile(filename.toStdString())) {
        refresh();
        emit patchesChanged();
        QMessageBox::information(this, "Success", 
            QString("Loaded %1 patches").arg(model_->rowCount()));
    } else {
        QMessageBox::warning(this, "Error", 
            QString::fromStdString(patchManager_->getLastError()));
    }
}

void PatchEditor::onSaveClicked() {
    if (!patchManager_) {
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, "Save Patches", 
        "", "JSON Files (*.json);;All Files (*)");
    
    if (filename.isEmpty()) {
        return;
    }

    if (patchManager_->saveToFile(filename.toStdString())) {
        QMessageBox::information(this, "Success", 
            QString("Saved %1 patches").arg(model_->rowCount()));
    } else {
        QMessageBox::warning(this, "Error", 
            QString::fromStdString(patchManager_->getLastError()));
    }
}

void PatchEditor::onTableDoubleClicked(const QModelIndex& index) {
    if (index.isValid()) {
        onEditClicked();
    }
}

} // namespace gui
} // namespace rebear
