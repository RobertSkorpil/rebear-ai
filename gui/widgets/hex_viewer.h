#pragma once

#include <QWidget>
#include <QDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>
#include <set>
#include <map>
#include "rebear/patch.h"
#include "rebear/patch_manager.h"

namespace rebear {
namespace gui {

/**
 * @brief Custom widget for displaying hex data with interactive patching
 */
class HexDisplay : public QWidget {
    Q_OBJECT

public:
    explicit HexDisplay(QWidget* parent = nullptr);

    // Load flash data from file
    bool loadFlashData(const std::string& filename);

    // Set the patch manager
    void setPatchManager(rebear::PatchManager* manager);

    // Navigate to address
    void gotoAddress(uint32_t address);

    // Highlight a range (e.g., from transaction)
    void highlightRange(uint32_t address, uint32_t count);

    // Clear highlights
    void clearHighlights();

    // Refresh display (e.g., after patches change)
    void refresh();

signals:
    // Emitted when user creates/modifies a patch
    void patchCreated(const rebear::Patch& patch);

    // Emitted when user wants to apply patches
    void applyPatchesRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void calculateLayout();
    void drawHexView(QPainter& painter);
    void drawAddressColumn(QPainter& painter);
    void drawHexColumn(QPainter& painter);
    void drawAsciiColumn(QPainter& painter);
    
    uint32_t getByteAtPosition(const QPoint& pos) const;
    QRect getByteRect(uint32_t address) const;
    
    void showByteEditDialog(uint32_t address);
    uint8_t getNextAvailablePatchId() const;

    // Data
    std::vector<uint8_t> flashData_;
    rebear::PatchManager* patchManager_;
    
    // View state
    uint32_t scrollOffset_;      // Current scroll position (in bytes)
    uint32_t bytesPerRow_;       // Bytes per row (typically 16)
    uint32_t visibleRows_;       // Number of visible rows
    
    // Highlighting
    struct Highlight {
        uint32_t address;
        uint32_t count;
        QColor color;
    };
    std::vector<Highlight> highlights_;
    
    // Layout metrics
    int charWidth_;
    int charHeight_;
    int addressColumnWidth_;
    int hexColumnWidth_;
    int asciiColumnWidth_;
    int rowHeight_;
    
    // Mouse interaction
    uint32_t hoveredByte_;
    bool isHovering_;
};

/**
 * @brief Complete hex viewer widget with controls
 */
class HexViewer : public QWidget {
    Q_OBJECT

public:
    explicit HexViewer(QWidget* parent = nullptr);

    // Load flash data
    bool loadFlashData(const std::string& filename);

    // Set the patch manager
    void setPatchManager(rebear::PatchManager* manager);

    // Navigate to address
    void gotoAddress(uint32_t address);

    // Highlight transaction data
    void highlightTransaction(uint32_t address, uint32_t count);

    // Refresh display
    void refresh();

signals:
    // Emitted when user creates a patch
    void patchCreated(const rebear::Patch& patch);

    // Emitted when user wants to apply patches
    void applyPatchesRequested();

public slots:
    // Handle transaction clicks from TransactionViewer
    void onTransactionClicked(uint32_t address);

private slots:
    void onGotoClicked();
    void onSearchClicked();
    void onClearHighlightsClicked();
    void onLoadFlashClicked();

private:
    void setupUi();

    // Widgets
    HexDisplay* hexDisplay_;
    QLineEdit* editGotoAddress_;
    QPushButton* btnGoto_;
    QLineEdit* editSearch_;
    QPushButton* btnSearch_;
    QPushButton* btnClearHighlights_;
    QPushButton* btnLoadFlash_;
    QLabel* lblStatus_;
    QScrollBar* scrollBar_;
};

/**
 * @brief Dialog for editing a byte and creating/modifying a patch
 */
class ByteEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit ByteEditDialog(uint32_t address, uint8_t currentValue, 
                           uint8_t originalValue, uint8_t patchId,
                           QWidget* parent = nullptr);

    // Get the patch data (8 bytes starting at address)
    std::array<uint8_t, 8> getPatchData() const;

    // Get patch ID
    uint8_t getPatchId() const { return patchId_; }

private slots:
    void onByteValueChanged();
    void onApplyClicked();

private:
    void setupUi();

    uint32_t address_;
    uint8_t originalValue_;
    uint8_t patchId_;
    
    // Widgets
    QLabel* lblAddress_;
    QLabel* lblOriginal_;
    QLabel* lblCurrent_;
    QSpinBox* spinPatchId_;
    QLineEdit* editByte0_;
    QLineEdit* editByte1_;
    QLineEdit* editByte2_;
    QLineEdit* editByte3_;
    QLineEdit* editByte4_;
    QLineEdit* editByte5_;
    QLineEdit* editByte6_;
    QLineEdit* editByte7_;
    QTextEdit* txtPreview_;
    QPushButton* btnApply_;
    QPushButton* btnCancel_;
    
    std::array<QLineEdit*, 8> byteEdits_;
};

} // namespace gui
} // namespace rebear
