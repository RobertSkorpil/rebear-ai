#pragma once

#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QPushButton>
#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <map>
#include "rebear/patch.h"
#include "rebear/patch_manager.h"

namespace rebear {
namespace gui {

/**
 * @brief Model for displaying patches in a QTableView
 */
class PatchModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PatchModel(QObject* parent = nullptr);

    // Set the patch manager
    void setPatchManager(rebear::PatchManager* manager);

    // Refresh from patch manager
    void refresh();

    // Get patch at row
    const rebear::Patch* getPatch(int row) const;

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    rebear::PatchManager* patchManager_;
    std::vector<rebear::Patch> patches_;
    
    // Column indices
    enum Column {
        COL_ID = 0,
        COL_ADDRESS,
        COL_DATA,
        COL_ENABLED,
        COL_COUNT_MAX
    };
};

/**
 * @brief Dialog for adding/editing a patch
 */
class PatchDialog : public QDialog {
    Q_OBJECT

public:
    explicit PatchDialog(QWidget* parent = nullptr);

    // Set patch data (for editing)
    void setPatch(const rebear::Patch& patch);

    // Get patch data
    rebear::Patch getPatch() const;

private slots:
    void onDataTextChanged();
    void validateAndAccept();

private:
    void setupUi();
    bool validateData();

    // Widgets
    QSpinBox* spinId_;
    QLineEdit* editAddress_;
    QLineEdit* editData_;
    QLabel* lblDataPreview_;
    QPushButton* btnOk_;
    QPushButton* btnCancel_;
};

/**
 * @brief Widget for editing and managing patches
 */
class PatchEditor : public QWidget {
    Q_OBJECT

public:
    explicit PatchEditor(QWidget* parent = nullptr);

    // Set the patch manager
    void setPatchManager(rebear::PatchManager* manager);

    // Refresh the view
    void refresh();

signals:
    // Emitted when patches are modified
    void patchesChanged();

    // Emitted when user wants to apply all patches
    void applyAllRequested();

    // Emitted when user wants to clear all patches
    void clearAllRequested();

private slots:
    void onAddClicked();
    void onEditClicked();
    void onRemoveClicked();
    void onApplyAllClicked();
    void onClearAllClicked();
    void onLoadClicked();
    void onSaveClicked();
    void onTableDoubleClicked(const QModelIndex& index);

private:
    void setupUi();

    // Widgets
    QTableView* tableView_;
    PatchModel* model_;
    QPushButton* btnAdd_;
    QPushButton* btnEdit_;
    QPushButton* btnRemove_;
    QPushButton* btnApplyAll_;
    QPushButton* btnClearAll_;
    QPushButton* btnLoad_;
    QPushButton* btnSave_;

    // State
    rebear::PatchManager* patchManager_;
};

} // namespace gui
} // namespace rebear
