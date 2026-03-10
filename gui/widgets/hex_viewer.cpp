#include "hex_viewer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QTimer>
#include <QMenu>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace rebear {
namespace gui {

// ============================================================================
// HexDisplay - Inline Editing Implementation
// ============================================================================

HexDisplay::HexDisplay(QWidget* parent)
    : QWidget(parent)
    , patchManager_(nullptr)
    , scrollOffset_(0)
    , bytesPerRow_(16)
    , visibleRows_(0)
    , hoveredByte_(0xFFFFFFFF)
    , isHovering_(false)
    , hasSelection_(false)
    , selectionStart_(0)
    , selectionEnd_(0)
    , isEditing_(false)
    , editAddress_(0xFFFFFFFF)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    QFont font("Monospace", 10);
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);
    
    calculateLayout();
    setMinimumSize(800, 400);
}

bool HexDisplay::loadFlashData(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    flashData_.resize(size);
    file.read(reinterpret_cast<char*>(flashData_.data()), size);
    
    scrollOffset_ = 0;
    modifiedBytes_.clear();
    update();
    return true;
}

void HexDisplay::setFlashData(const std::vector<uint8_t>& data) {
    flashData_ = data;
    scrollOffset_ = 0;
    modifiedBytes_.clear();
    update();
}

void HexDisplay::setPatchManager(rebear::PatchManager* manager) {
    patchManager_ = manager;
    update();
}

void HexDisplay::gotoAddress(uint32_t address) {
    if (address < flashData_.size()) {
        scrollOffset_ = (address / bytesPerRow_) * bytesPerRow_;
        update();
    }
}

void HexDisplay::highlightRange(uint32_t address, uint32_t count) {
    Highlight h;
    h.address = address;
    h.count = count;
    h.color = QColor(255, 255, 0, 100);
    highlights_.push_back(h);
    update();
}

void HexDisplay::clearHighlights() {
    highlights_.clear();
    update();
}

void HexDisplay::refresh() {
    update();
}

void HexDisplay::setByteValue(uint32_t address, uint8_t value) {
    if (address >= flashData_.size()) return;
    
    if (value == flashData_[address]) {
        // Same as original, remove modification
        modifiedBytes_.erase(address);
    } else {
        modifiedBytes_[address] = value;
    }
    
    update();
    emit modificationsChanged();
}

uint8_t HexDisplay::getByteValue(uint32_t address) const {
    if (address >= flashData_.size()) return 0xFF;
    
    auto it = modifiedBytes_.find(address);
    if (it != modifiedBytes_.end()) {
        return it->second;
    }
    return flashData_[address];
}

void HexDisplay::clearModifications() {
    modifiedBytes_.clear();
    update();
    emit modificationsChanged();
}

void HexDisplay::applyModificationsAsPatches() {
    // This will be called by the parent widget
    // Parent handles optimization and patch creation
}

void HexDisplay::calculateLayout() {
    QFontMetrics fm(font());
    charWidth_ = fm.horizontalAdvance('0');
    charHeight_ = fm.height();
    rowHeight_ = charHeight_ + 4;
    
    addressColumnWidth_ = charWidth_ * 10;
    hexColumnWidth_ = charWidth_ * (bytesPerRow_ * 3 + 2);
    asciiColumnWidth_ = charWidth_ * (bytesPerRow_ + 2);
    
    visibleRows_ = (height() - 20) / rowHeight_;
}

void HexDisplay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    
    if (flashData_.empty()) {
        painter.drawText(rect(), Qt::AlignCenter, "No flash data loaded");
        return;
    }
    
    drawAddressColumn(painter);
    drawHexColumn(painter);
    drawAsciiColumn(painter);
}

void HexDisplay::drawAddressColumn(QPainter& painter) {
    painter.setPen(Qt::darkGray);
    
    for (uint32_t row = 0; row < visibleRows_; ++row) {
        uint32_t address = scrollOffset_ + (row * bytesPerRow_);
        if (address >= flashData_.size()) break;
        
        int y = 10 + row * rowHeight_;
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') 
            << std::uppercase << address << ":";
        
        painter.drawText(5, y + charHeight_, QString::fromStdString(oss.str()));
    }
}

void HexDisplay::drawHexColumn(QPainter& painter) {
    int hexStartX = addressColumnWidth_ + 10;
    
    // Get active patches from patch manager
    std::map<uint32_t, rebear::Patch> patchMap;
    if (patchManager_) {
        auto patches = patchManager_->getPatches();
        for (const auto& patch : patches) {
            if (patch.enabled && patch.address < flashData_.size()) {
                size_t patchLen = std::min(patch.data.size(), 
                                          size_t(flashData_.size() - patch.address));
                for (size_t i = 0; i < patchLen; ++i) {
                    patchMap[patch.address + static_cast<uint32_t>(i)] = patch;
                }
            }
        }
    }
    
    for (uint32_t row = 0; row < visibleRows_; ++row) {
        uint32_t rowAddress = scrollOffset_ + (row * bytesPerRow_);
        if (rowAddress >= flashData_.size()) break;
        
        int y = 10 + row * rowHeight_;
        
        for (uint32_t col = 0; col < bytesPerRow_; ++col) {
            uint32_t address = rowAddress + col;
            if (address >= flashData_.size()) break;
            
            int x = hexStartX + col * charWidth_ * 3;
            QRect byteRect(x, y, charWidth_ * 2, charHeight_);
            
            // Check if currently editing this byte
            bool isEditingThis = (isEditing_ && address == editAddress_);
            
            // Check if in selection
            bool isSelected = false;
            if (hasSelection_) {
                uint32_t selMin = std::min(selectionStart_, selectionEnd_);
                uint32_t selMax = std::max(selectionStart_, selectionEnd_);
                isSelected = (address >= selMin && address <= selMax);
            }
            
            // Check highlights
            for (const auto& h : highlights_) {
                if (address >= h.address && address < h.address + h.count) {
                    painter.fillRect(byteRect, h.color);
                    break;
                }
            }
            
            // Selection highlight (before other highlights)
            if (isSelected && !isEditingThis) {
                painter.fillRect(byteRect, QColor(173, 216, 230, 150));  // Light blue for selection
            }
            
            // Check if modified by user
            bool isModified = modifiedBytes_.find(address) != modifiedBytes_.end();
            if (isModified) {
                painter.fillRect(byteRect, QColor(255, 200, 100, 150));  // Orange for user modifications
            }
            
            // Check if patched by patch manager
            bool isPatched = patchMap.find(address) != patchMap.end();
            if (isPatched && !isModified) {
                painter.fillRect(byteRect, QColor(255, 200, 200, 150));  // Red for applied patches
            }
            
            // Check if hovered
            if (isHovering_ && address == hoveredByte_ && !isEditingThis && !isSelected) {
                painter.fillRect(byteRect, QColor(200, 200, 255, 150));  // Blue for hover
            }
            
            // Editing highlight (highest priority)
            if (isEditingThis) {
                painter.fillRect(byteRect, QColor(100, 255, 100, 200));  // Bright green for editing
            }
            
            // Get byte value (modified or original)
            uint8_t byte = getByteValue(address);
            
            // Set color
            if (isModified) {
                painter.setPen(QColor(200, 100, 0));  // Orange text
            } else if (isPatched) {
                painter.setPen(Qt::red);
            } else {
                painter.setPen(Qt::black);
            }
            
            // Draw value
            if (isEditingThis && !editBuffer_.isEmpty()) {
                // Show edit buffer
                painter.setPen(Qt::darkGreen);
                painter.drawText(x, y + charHeight_, editBuffer_.leftJustified(2, '_'));
            } else {
                std::ostringstream oss;
                oss << std::hex << std::setw(2) << std::setfill('0') 
                    << std::uppercase << static_cast<int>(byte);
                painter.drawText(x, y + charHeight_, QString::fromStdString(oss.str()));
            }
        }
    }
}

void HexDisplay::drawAsciiColumn(QPainter& painter) {
    int asciiStartX = addressColumnWidth_ + hexColumnWidth_ + 20;
    painter.setPen(Qt::darkGray);
    
    for (uint32_t row = 0; row < visibleRows_; ++row) {
        uint32_t rowAddress = scrollOffset_ + (row * bytesPerRow_);
        if (rowAddress >= flashData_.size()) break;
        
        int y = 10 + row * rowHeight_;
        painter.drawText(asciiStartX, y + charHeight_, "|");
        
        for (uint32_t col = 0; col < bytesPerRow_; ++col) {
            uint32_t address = rowAddress + col;
            if (address >= flashData_.size()) break;
            
            uint8_t byte = getByteValue(address);
            char c = (byte >= 32 && byte < 127) ? byte : '.';
            
            int x = asciiStartX + charWidth_ + col * charWidth_;
            painter.drawText(x, y + charHeight_, QString(QChar(c)));
        }
        
        painter.drawText(asciiStartX + charWidth_ * (bytesPerRow_ + 1), 
                        y + charHeight_, "|");
    }
}

void HexDisplay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        uint32_t address = getByteAtPosition(event->pos());
        if (address != 0xFFFFFFFF && address < flashData_.size()) {
            if (event->modifiers() & Qt::ShiftModifier && hasSelection_) {
                // Extend selection
                selectionEnd_ = address;
                update();
            } else {
                // Start new selection or edit
                hasSelection_ = true;
                selectionStart_ = address;
                selectionEnd_ = address;
                
                // Start editing if single click
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    startEditing(address);
                }
                update();
            }
        }
    } else if (event->button() == Qt::RightButton) {
        uint32_t address = getByteAtPosition(event->pos());
        if (address != 0xFFFFFFFF && address < flashData_.size()) {
            showContextMenu(event->globalPosition().toPoint(), address);
        }
    }
}

void HexDisplay::mouseMoveEvent(QMouseEvent* event) {
    uint32_t address = getByteAtPosition(event->pos());
    
    // Handle selection drag
    if (event->buttons() & Qt::LeftButton && hasSelection_) {
        if (address != 0xFFFFFFFF && address < flashData_.size()) {
            selectionEnd_ = address;
            update();
        }
    }
    
    // Handle hover
    if (address != hoveredByte_) {
        hoveredByte_ = address;
        isHovering_ = (address != 0xFFFFFFFF && address < flashData_.size());
        update();
    }
}

void HexDisplay::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    int rows = delta > 0 ? -3 : 3;
    
    int64_t newOffset = scrollOffset_ + (rows * bytesPerRow_);
    if (newOffset < 0) newOffset = 0;
    if (newOffset >= static_cast<int64_t>(flashData_.size())) {
        newOffset = (flashData_.size() / bytesPerRow_) * bytesPerRow_;
    }
    
    scrollOffset_ = static_cast<uint32_t>(newOffset);
    update();
}

void HexDisplay::keyPressEvent(QKeyEvent* event) {
    // Copy selection (Ctrl+C)
    if (event->matches(QKeySequence::Copy) && hasSelection_) {
        uint32_t selMin = std::min(selectionStart_, selectionEnd_);
        uint32_t selMax = std::max(selectionStart_, selectionEnd_);
        
        QString hexStr;
        for (uint32_t addr = selMin; addr <= selMax && addr < flashData_.size(); ++addr) {
            if (!hexStr.isEmpty()) hexStr += " ";
            hexStr += QString("%1").arg(getByteValue(addr), 2, 16, QChar('0')).toUpper();
        }
        
        QApplication::clipboard()->setText(hexStr);
        return;
    }
    
    // Paste (Ctrl+V)
    if (event->matches(QKeySequence::Paste) && hasSelection_) {
        QString clipText = QApplication::clipboard()->text();
        // Split on whitespace
        QStringList bytes = clipText.split(' ', Qt::SkipEmptyParts);
        
        uint32_t addr = selectionStart_;
        for (const QString& byteStr : bytes) {
            if (addr >= flashData_.size()) break;
            
            bool ok;
            uint8_t value = static_cast<uint8_t>(byteStr.toUInt(&ok, 16));
            if (ok) {
                setByteValue(addr, value);
            }
            addr++;
        }
        return;
    }
    
    // Select all (Ctrl+A)
    if (event->matches(QKeySequence::SelectAll)) {
        hasSelection_ = true;
        selectionStart_ = 0;
        selectionEnd_ = static_cast<uint32_t>(flashData_.size() - 1);
        update();
        return;
    }
    
    // Escape - clear selection
    if (event->key() == Qt::Key_Escape) {
        if (isEditing_) {
            cancelEdit();
        } else if (hasSelection_) {
            hasSelection_ = false;
            update();
        }
        return;
    }
    
    // Editing keys
    if (!isEditing_) {
        QWidget::keyPressEvent(event);
        return;
    }
    
    QString key = event->text().toUpper();
    
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commitEdit();
    } else if (event->key() == Qt::Key_Backspace) {
        if (!editBuffer_.isEmpty()) {
            editBuffer_.chop(1);
            update();
        }
    } else if (key.length() == 1 && editBuffer_.length() < 2) {
        QChar c = key[0];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
            editBuffer_ += c;
            if (editBuffer_.length() == 2) {
                // Auto-commit after 2 hex digits
                commitEdit();
            } else {
                update();
            }
        }
    }
}

void HexDisplay::resizeEvent(QResizeEvent* event) {
    calculateLayout();
    QWidget::resizeEvent(event);
}

uint32_t HexDisplay::getByteAtPosition(const QPoint& pos) const {
    int hexStartX = addressColumnWidth_ + 10;
    
    if (pos.x() < hexStartX || pos.x() > hexStartX + hexColumnWidth_) {
        return 0xFFFFFFFF;
    }
    
    int row = (pos.y() - 10) / rowHeight_;
    if (row < 0 || row >= static_cast<int>(visibleRows_)) {
        return 0xFFFFFFFF;
    }
    
    int relX = pos.x() - hexStartX;
    int col = relX / (charWidth_ * 3);
    if (col < 0 || col >= static_cast<int>(bytesPerRow_)) {
        return 0xFFFFFFFF;
    }
    
    uint32_t address = scrollOffset_ + (row * bytesPerRow_) + col;
    return address;
}

QRect HexDisplay::getByteRect(uint32_t address) const {
    if (address < scrollOffset_ || address >= scrollOffset_ + (visibleRows_ * bytesPerRow_)) {
        return QRect();
    }
    
    uint32_t offset = address - scrollOffset_;
    uint32_t row = offset / bytesPerRow_;
    uint32_t col = offset % bytesPerRow_;
    
    int hexStartX = addressColumnWidth_ + 10;
    int x = hexStartX + col * charWidth_ * 3;
    int y = 10 + row * rowHeight_;
    
    return QRect(x, y, charWidth_ * 2, charHeight_);
}

void HexDisplay::startEditing(uint32_t address) {
    if (isEditing_) {
        commitEdit();
    }
    
    isEditing_ = true;
    editAddress_ = address;
    editBuffer_.clear();
    update();
}

void HexDisplay::commitEdit() {
    if (!isEditing_) return;
    
    if (editBuffer_.length() == 2) {
        bool ok;
        uint8_t value = static_cast<uint8_t>(editBuffer_.toUInt(&ok, 16));
        if (ok) {
            setByteValue(editAddress_, value);
        }
    }
    
    isEditing_ = false;
    editBuffer_.clear();
    update();
}

void HexDisplay::cancelEdit() {
    isEditing_ = false;
    editBuffer_.clear();
    update();
}

void HexDisplay::showContextMenu(const QPoint& globalPos, uint32_t address) {
    QMenu menu;
    
    // Check if byte is modified
    bool isModified = modifiedBytes_.find(address) != modifiedBytes_.end();
    
    // Check if byte is patched
    bool isPatched = false;
    if (patchManager_) {
        auto patches = patchManager_->getPatches();
        for (const auto& patch : patches) {
            if (patch.enabled && address >= patch.address && 
                address < patch.address + patch.data.size()) {
                isPatched = true;
                break;
            }
        }
    }
    
    uint8_t currentValue = getByteValue(address);
    uint8_t originalValue = flashData_[address];
    
    // Show current and original values
    QString info = QString("Address: 0x%1\nCurrent: 0x%2\nOriginal: 0x%3")
        .arg(address, 6, 16, QChar('0')).toUpper()
        .arg(currentValue, 2, 16, QChar('0')).toUpper()
        .arg(originalValue, 2, 16, QChar('0')).toUpper();
    
    QAction* infoAction = menu.addAction(info);
    infoAction->setEnabled(false);
    
    menu.addSeparator();
    
    // Restore original value option
    if (isModified) {
        QAction* restoreAction = menu.addAction("Restore Original Value");
        connect(restoreAction, &QAction::triggered, [this, address]() {
            modifiedBytes_.erase(address);
            update();
            emit modificationsChanged();
        });
    }
    
    // Show patch info if byte is patched
    if (isPatched) {
        QAction* patchedAction = menu.addAction("(Byte is patched by Patch Manager)");
        patchedAction->setEnabled(false);
    }
    
    menu.exec(globalPos);
}

// ============================================================================
// HexViewer - Main Widget
// ============================================================================

HexViewer::HexViewer(QWidget* parent)
    : QWidget(parent)
    , autoApplyEnabled_(true)
{
    setupUi();
    
    autoApplyTimer_ = new QTimer(this);
    autoApplyTimer_->setSingleShot(true);
    autoApplyTimer_->setInterval(300);
    connect(autoApplyTimer_, &QTimer::timeout, this, &HexViewer::applyModificationsAutomatically);
}

void HexViewer::setupUi() {
    hexDisplay_ = new HexDisplay(this);
    
    // Controls
    editGotoAddress_ = new QLineEdit(this);
    editGotoAddress_->setPlaceholderText("Address (hex)");
    editGotoAddress_->setMaxLength(8);
    
    btnGoto_ = new QPushButton("Go To", this);
    connect(btnGoto_, &QPushButton::clicked, this, &HexViewer::onGotoClicked);
    
    // Connect Enter key to Go To button
    connect(editGotoAddress_, &QLineEdit::returnPressed, this, &HexViewer::onGotoClicked);
    
    btnClearHighlights_ = new QPushButton("Clear Highlights", this);
    connect(btnClearHighlights_, &QPushButton::clicked, this, &HexViewer::onClearHighlightsClicked);
    
    btnLoadFlash_ = new QPushButton("Load Flash...", this);
    connect(btnLoadFlash_, &QPushButton::clicked, this, &HexViewer::onLoadFlashClicked);
    
    lblStatus_ = new QLabel("No flash data loaded", this);
    
    // Modification panel
    txtModifications_ = new QTextEdit(this);
    txtModifications_->setReadOnly(true);
    txtModifications_->setMaximumHeight(150);
    txtModifications_->setStyleSheet("QTextEdit { background-color: #f5f5f5; font-family: monospace; font-size: 9pt; }");
    
    lblModCount_ = new QLabel("No modifications", this);
    lblModCount_->setStyleSheet("QLabel { font-weight: bold; }");
    
    btnApplyModifications_ = new QPushButton("Create Patches from Modifications", this);
    btnApplyModifications_->setEnabled(false);
    btnApplyModifications_->setToolTip("Generate patch ranges and add to Patches panel (does not send to device yet)");
    connect(btnApplyModifications_, &QPushButton::clicked, this, &HexViewer::onApplyModifications);
    
    btnClearModifications_ = new QPushButton("Clear Modifications", this);
    btnClearModifications_->setEnabled(false);
    connect(btnClearModifications_, &QPushButton::clicked, this, &HexViewer::onClearModifications);
    
    btnGenerateCommand_ = new QPushButton("Copy CLI Command", this);
    btnGenerateCommand_->setEnabled(false);
    btnGenerateCommand_->setToolTip("Generate rebear-cli command and copy to clipboard");
    connect(btnGenerateCommand_, &QPushButton::clicked, this, &HexViewer::onGenerateCommand);
    
    // Layout
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel("Address:", this));
    controlLayout->addWidget(editGotoAddress_);
    controlLayout->addWidget(btnGoto_);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(btnClearHighlights_);
    controlLayout->addStretch();
    controlLayout->addWidget(btnLoadFlash_);
    
    QVBoxLayout* modPanel = new QVBoxLayout();
    modPanel->addWidget(lblModCount_);
    modPanel->addWidget(txtModifications_);
    QHBoxLayout* modButtons = new QHBoxLayout();
    modButtons->addWidget(btnApplyModifications_);
    modButtons->addWidget(btnClearModifications_);
    modButtons->addWidget(btnGenerateCommand_);
    modButtons->addStretch();
    modPanel->addLayout(modButtons);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(hexDisplay_, 1);
    mainLayout->addWidget(new QLabel("Modifications (click bytes to edit, type hex values):", this));
    mainLayout->addLayout(modPanel);
    mainLayout->addWidget(lblStatus_);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    setLayout(mainLayout);
    
    connect(hexDisplay_, &HexDisplay::modificationsChanged, this, &HexViewer::onModificationsChanged);
    connect(hexDisplay_, &HexDisplay::patchCreated, this, &HexViewer::patchCreated);
}

bool HexViewer::loadFlashData(const std::string& filename) {
    if (hexDisplay_->loadFlashData(filename)) {
        lblStatus_->setText(QString("Loaded: %1").arg(QString::fromStdString(filename)));
        onModificationsChanged();
        return true;
    }
    lblStatus_->setText("Failed to load flash data");
    return false;
}

void HexViewer::setFlashData(const std::vector<uint8_t>& data) {
    hexDisplay_->setFlashData(data);
    lblStatus_->setText(QString("Data loaded: %1 bytes").arg(data.size()));
    onModificationsChanged();
}

void HexViewer::setAutoApplyEnabled(bool enabled) {
    autoApplyEnabled_ = enabled;
    if (!enabled) {
        autoApplyTimer_->stop();
    }
}

void HexViewer::setPatchManager(rebear::PatchManager* manager) {
    hexDisplay_->setPatchManager(manager);
}

void HexViewer::gotoAddress(uint32_t address) {
    hexDisplay_->gotoAddress(address);
    
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << address;
    editGotoAddress_->setText(QString::fromStdString(oss.str()));
}

void HexViewer::highlightTransaction(uint32_t address, uint32_t count) {
    hexDisplay_->highlightRange(address, count);
    gotoAddress(address);
}

void HexViewer::refresh() {
    hexDisplay_->refresh();
}

void HexViewer::onTransactionClicked(uint32_t address) {
    gotoAddress(address);
}

void HexViewer::onGotoClicked() {
    bool ok;
    uint32_t address = editGotoAddress_->text().toUInt(&ok, 16);
    if (ok) {
        gotoAddress(address);
    } else {
        QMessageBox::warning(this, "Invalid Address", "Please enter a valid hex address");
    }
}

void HexViewer::onClearHighlightsClicked() {
    hexDisplay_->clearHighlights();
}

void HexViewer::onLoadFlashClicked() {
    QString filename = QFileDialog::getOpenFileName(this, "Load Flash Data",
        "data", "Binary Files (*.bin);;All Files (*)");
    
    if (!filename.isEmpty()) {
        loadFlashData(filename.toStdString());
    }
}

void HexViewer::onModificationsChanged() {
    updateModificationPanel();
    
    if (autoApplyEnabled_) {
        autoApplyTimer_->start();
    }
}

void HexViewer::updateModificationPanel() {
    auto mods = hexDisplay_->getModifiedBytes();
    
    if (mods.empty()) {
        lblModCount_->setText("No modifications");
        lblModCount_->setStyleSheet("QLabel { font-weight: bold; color: gray; }");
        txtModifications_->setText("Click on any byte in the hex view to start editing.\nType 2 hex digits to change the value.");
        btnApplyModifications_->setEnabled(false);
        btnClearModifications_->setEnabled(false);
        btnGenerateCommand_->setEnabled(false);
        return;
    }
    
    // Calculate optimized patches
    auto patches = calculateOptimizedPatches();
    
    // Check if too many patches
    bool tooManyPatches = patches.size() > 8;
    
    if (tooManyPatches) {
        lblModCount_->setText(QString("⚠ WARNING: %1 byte(s) modified → %2 patches (MAX 8!)")
                             .arg(mods.size())
                             .arg(patches.size()));
        lblModCount_->setStyleSheet("QLabel { font-weight: bold; color: red; }");
    } else {
        lblModCount_->setText(QString("✓ %1 byte(s) modified → %2 optimized patch(es)")
                             .arg(mods.size())
                             .arg(patches.size()));
        lblModCount_->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    }
    
    // Show patch details
    std::ostringstream oss;
    
    if (tooManyPatches) {
        oss << "⚠ ERROR: Too many patch ranges (" << patches.size() << " > 8)\n";
        oss << "The FPGA hardware only supports 8 patches maximum per buffer.\n";
        oss << "Please reduce your modifications or group them closer together.\n\n";
    }
    
    oss << "Optimized Patch Ranges:\n\n";
    
    for (size_t i = 0; i < patches.size(); ++i) {
        oss << "Patch " << (i+1) << ": 0x" << std::hex << std::setw(8) 
            << std::setfill('0') << std::uppercase << patches[i].address 
            << " (" << std::dec << patches[i].data.size() << " bytes)\n";
        
        oss << "  Data: ";
        size_t showBytes = std::min(patches[i].data.size(), size_t(16));
        for (size_t j = 0; j < showBytes; ++j) {
            oss << std::hex << std::setw(2) << std::setfill('0') 
                << std::uppercase << static_cast<int>(patches[i].data[j]) << " ";
        }
        if (patches[i].data.size() > 16) {
            oss << "... (+" << std::dec << (patches[i].data.size() - 16) << " more)";
        }
        oss << "\n\n";
    }
    
    txtModifications_->setText(QString::fromStdString(oss.str()));
    btnApplyModifications_->setEnabled(!tooManyPatches);
    btnClearModifications_->setEnabled(true);
    btnGenerateCommand_->setEnabled(true);
}

std::vector<rebear::Patch> HexViewer::calculateOptimizedPatches() const {
    auto mods = hexDisplay_->getModifiedBytes();
    if (mods.empty()) {
        return {};
    }
    
    std::vector<rebear::Patch> patches;
    std::vector<uint32_t> sortedAddrs;
    
    for (const auto& pair : mods) {
        sortedAddrs.push_back(pair.first);
    }
    std::sort(sortedAddrs.begin(), sortedAddrs.end());
    
    // Group into patches (split on gaps > 8 bytes, max 256 bytes/patch)
    const uint32_t MAX_GAP = 8;
    const size_t MAX_PATCH_SIZE = 256;
    
    uint32_t patchStart = sortedAddrs[0];
    uint32_t patchEnd = sortedAddrs[0] + 1;
    
    uint8_t nextId = 0;
    
    auto emitPatch = [&]() {
        rebear::Patch patch;
        patch.address = patchStart;
        patch.enabled = true;
        patch.id = nextId++;  // Assign unique ID
        
        for (uint32_t addr = patchStart; addr < patchEnd; ++addr) {
            patch.data.push_back(hexDisplay_->getByteValue(addr));
        }
        
        patches.push_back(patch);
    };
    
    for (size_t i = 1; i < sortedAddrs.size(); ++i) {
        uint32_t addr = sortedAddrs[i];
        uint32_t gap = addr - patchEnd;
        uint32_t newSize = addr - patchStart + 1;
        
        if (gap <= MAX_GAP && newSize <= MAX_PATCH_SIZE) {
            patchEnd = addr + 1;
        } else {
            emitPatch();
            patchStart = addr;
            patchEnd = addr + 1;
        }
    }
    
    emitPatch();
    return patches;
}

void HexViewer::onApplyModifications() {
    auto patches = calculateOptimizedPatches();
    if (patches.empty()) return;
    
    if (patches.size() > 8) {
        QMessageBox::critical(this, "Too Many Patches", 
            QString("Cannot apply: %1 patches would be created, but FPGA only supports 8 maximum per buffer.\n\n"
                   "Please reduce your modifications or group them closer together.")
                   .arg(patches.size()));
        return;
    }
    
    // Emit the patches - the main window will handle applying to device
    for (const auto& patch : patches) {
        emit patchCreated(patch);
    }
    
    // DON'T clear modifications - keep them visible in hex view
    // They'll still show as orange (modified) vs red (applied patches)
    // User can manually clear with "Clear Modifications" button
    
    QMessageBox::information(this, "Patches Created", 
        QString("Created %1 patch(es) from your modifications.\n\n"
               "The modifications are still visible (orange) in the hex view.\n"
               "Applied patches will show in red once sent to device.\n\n"
               "Use 'Apply All' in the Patches panel to send to device.")
               .arg(patches.size()));
}

void HexViewer::onClearModifications() {
    auto reply = QMessageBox::question(this, "Clear Modifications", 
                                       "Discard all modifications?");
    if (reply == QMessageBox::Yes) {
        hexDisplay_->clearModifications();
    }
}

void HexViewer::onGenerateCommand() {
    auto patches = calculateOptimizedPatches();
    if (patches.empty()) {
        return;
    }
    
    // Build single rebear-cli command with all patches
    QString command = "rebear-cli patch set";
    
    for (const auto& patch : patches) {
        command += QString(" --address 0x%1 --data ")
                   .arg(patch.address, 6, 16, QChar('0'));
        
        // Add hex data (uppercase)
        for (size_t i = 0; i < patch.data.size(); ++i) {
            command += QString("%1").arg(patch.data[i], 2, 16, QChar('0')).toUpper();
        }
    }
    
    // Copy to clipboard
    QApplication::clipboard()->setText(command);
    
    // Show confirmation with preview
    QString preview = command;
    if (preview.length() > 300) {
        preview = preview.left(300) + "...";
    }
    
    QMessageBox::information(this, "Command Copied", 
        QString("rebear-cli command copied to clipboard!\n\n"
               "Preview:\n%1\n\n"
               "Patches: %2 (sent in one buffer)")
               .arg(preview)
               .arg(patches.size()));
}

void HexViewer::applyModificationsAutomatically() {
    auto patches = calculateOptimizedPatches();
    if (patches.empty()) return;
    
    if (patches.size() > 8) {
        return;
    }
    
    for (const auto& patch : patches) {
        emit patchCreated(patch);
    }
    
    emit autoApplyPatches();
}

} // namespace gui
} // namespace rebear
