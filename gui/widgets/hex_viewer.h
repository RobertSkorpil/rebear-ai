#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
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
 * @brief Interactive hex editor with inline editing
 */
class HexDisplay : public QWidget {
    Q_OBJECT

public:
    explicit HexDisplay(QWidget* parent = nullptr);

    bool loadFlashData(const std::string& filename);
    void setPatchManager(rebear::PatchManager* manager);
    void gotoAddress(uint32_t address);
    void highlightRange(uint32_t address, uint32_t count);
    void clearHighlights();
    void refresh();
    
    // Editing
    void setByteValue(uint32_t address, uint8_t value);
    uint8_t getByteValue(uint32_t address) const;
    std::map<uint32_t, uint8_t> getModifiedBytes() const { return modifiedBytes_; }
    void clearModifications();
    void applyModificationsAsPatches();

signals:
    void modificationsChanged();  // Emitted when user edits bytes
    void patchCreated(const rebear::Patch& patch);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void calculateLayout();
    void drawAddressColumn(QPainter& painter);
    void drawHexColumn(QPainter& painter);
    void drawAsciiColumn(QPainter& painter);
    
    uint32_t getByteAtPosition(const QPoint& pos) const;
    QRect getByteRect(uint32_t address) const;
    
    void startEditing(uint32_t address);
    void commitEdit();
    void cancelEdit();

    // Data
    std::vector<uint8_t> flashData_;
    std::map<uint32_t, uint8_t> modifiedBytes_;  // Modified bytes overlay
    rebear::PatchManager* patchManager_;
    
    // View state
    uint32_t scrollOffset_;
    uint32_t bytesPerRow_;
    uint32_t visibleRows_;
    
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
    
    // Mouse/keyboard interaction
    uint32_t hoveredByte_;
    bool isHovering_;
    
    // Selection for copy/paste
    bool hasSelection_;
    uint32_t selectionStart_;
    uint32_t selectionEnd_;
    
    // Inline editing state
    bool isEditing_;
    uint32_t editAddress_;
    QString editBuffer_;
};

/**
 * @brief Complete hex viewer with modification panel
 */
class HexViewer : public QWidget {
    Q_OBJECT

public:
    explicit HexViewer(QWidget* parent = nullptr);

    bool loadFlashData(const std::string& filename);
    void setPatchManager(rebear::PatchManager* manager);
    void gotoAddress(uint32_t address);
    void highlightTransaction(uint32_t address, uint32_t count);
    void refresh();

signals:
    void patchCreated(const rebear::Patch& patch);
    void applyPatchesRequested();

public slots:
    void onTransactionClicked(uint32_t address);

private slots:
    void onGotoClicked();
    void onSearchClicked();
    void onClearHighlightsClicked();
    void onLoadFlashClicked();
    void onModificationsChanged();
    void onApplyModifications();
    void onClearModifications();
    void onGenerateCommand();

private:
    void setupUi();
    void updateModificationPanel();
    std::vector<rebear::Patch> calculateOptimizedPatches() const;

    // Widgets
    HexDisplay* hexDisplay_;
    QLineEdit* editGotoAddress_;
    QPushButton* btnGoto_;
    QLineEdit* editSearch_;
    QPushButton* btnSearch_;
    QPushButton* btnClearHighlights_;
    QPushButton* btnLoadFlash_;
    QLabel* lblStatus_;
    
    // Modification panel
    QTextEdit* txtModifications_;
    QPushButton* btnApplyModifications_;
    QPushButton* btnClearModifications_;
    QPushButton* btnGenerateCommand_;
    QLabel* lblModCount_;
};

} // namespace gui
} // namespace rebear
