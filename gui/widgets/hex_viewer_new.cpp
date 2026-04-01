#include "hex_viewer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QInputDialog>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace rebear {
namespace gui {

// ============================================================================
// HexDisplay Implementation
// ============================================================================

HexDisplay::HexDisplay(QWidget* parent)
    : QWidget(parent)
    , patches_(nullptr)
    , scrollOffset_(0)
    , bytesPerRow_(16)
    , visibleRows_(0)
    , hoveredByte_(0xFFFFFFFF)
    , isHovering_(false)
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
    update();
    return true;
}

void HexDisplay::setPatches(std::vector<rebear::Patch>* patches) {
    patches_ = patches;
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

void HexDisplay::calculateLayout() {
    QFontMetrics fm(font());
    charWidth_ = fm.horizontalAdvance('0');
    charHeight_ = fm.height();
    rowHeight_ = charHeight_ + 4;
    
    addressColumnWidth_ = charWidth_ * 10;
    hexColumnWidth_ = charWidth_ * (bytesPerRow_ * 3 + 2);
    asciiColumnWidth_ = 0;  // No longer using ASCII column
    
    visibleRows_ = (height() - 30) / rowHeight_;  // Extra space for header row
}

void HexDisplay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    
    if (flashData_.empty()) {
        painter.drawText(rect(), Qt::AlignCenter, "No flash data loaded");
        return;
    }
    
    drawColumnHeaders(painter);
    drawAddressColumn(painter);
    drawHexColumn(painter);
}

void HexDisplay::drawAddressColumn(QPainter& painter) {
    painter.setPen(Qt::darkGray);
    
    for (uint32_t row = 0; row < visibleRows_; ++row) {
        uint32_t address = scrollOffset_ + (row * bytesPerRow_);
        if (address >= flashData_.size()) break;
        
        int y = 30 + row * rowHeight_;  // Start after header row
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') 
            << std::uppercase << address << ":";
        
        painter.drawText(5, y + charHeight_, QString::fromStdString(oss.str()));
    }
}

void HexDisplay::drawColumnHeaders(QPainter& painter) {
    int hexStartX = addressColumnWidth_ + 10;
    
    painter.setPen(Qt::black);
    QFont headerFont = font();
    headerFont.setBold(true);
    painter.setFont(headerFont);
    
    // Draw header background
    QRect headerRect(0, 0, width(), 25);
    painter.fillRect(headerRect, QColor(240, 240, 240));
    
    // Draw "Offset" label above address column
    painter.drawText(5, 18, "Offset");
    
    // Draw column headers (0-F for last nibble)
    for (uint32_t col = 0; col < bytesPerRow_; ++col) {
        int x = hexStartX + col * charWidth_ * 3;
        char nibble = (col < 10) ? ('0' + col) : ('A' + col - 10);
        painter.drawText(x + charWidth_ / 2, 18, QString(QChar(nibble)));
    }
    
    // Restore normal font
    painter.setFont(font());
}

void HexDisplay::drawHexColumn(QPainter& painter) {
    int hexStartX = addressColumnWidth_ + 10;
    
    // Get active patches
    std::map<uint32_t, rebear::Patch> patchMap;
    if (patches_) {
        for (const auto& patch : *patches_) {
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
        
        int y = 30 + row * rowHeight_;  // Start after header row
        
        for (uint32_t col = 0; col < bytesPerRow_; ++col) {
            uint32_t address = rowAddress + col;
            if (address >= flashData_.size()) break;
            
            int x = hexStartX + col * charWidth_ * 3;
            QRect byteRect(x, y, charWidth_ * 2, charHeight_);
            
            // Check highlights
            for (const auto& h : highlights_) {
                if (address >= h.address && address < h.address + h.count) {
                    painter.fillRect(byteRect, h.color);
                    break;
                }
            }
            
            // Check if patched
            bool isPatched = patchMap.find(address) != patchMap.end();
            if (isPatched) {
                painter.fillRect(byteRect, QColor(255, 200, 200));
            }
            
            // Check if hovered
            if (isHovering_ && address == hoveredByte_) {
                painter.fillRect(byteRect, QColor(200, 200, 255));
            }
            
            // Draw byte value
            uint8_t byte = flashData_[address];
            if (isPatched) {
                auto it = patchMap.find(address);
                const auto& patch = it->second;
                size_t offset = address - patch.address;
                if (offset < patch.data.size()) {
                    byte = patch.data[offset];
                }
                painter.setPen(Qt::red);
            } else {
                painter.setPen(Qt::black);
            }
            
            std::ostringstream oss;
            oss << std::hex << std::setw(2) << std::setfill('0') 
                << std::uppercase << static_cast<int>(byte);
            
            painter.drawText(x, y + charHeight_, QString::fromStdString(oss.str()));
        }
    }
}

void HexDisplay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        uint32_t address = getByteAtPosition(event->pos());
        if (address != 0xFFFFFFFF && address < flashData_.size()) {
            showByteEditDialog(address);
        }
    }
}

void HexDisplay::mouseMoveEvent(QMouseEvent* event) {
    uint32_t address = getByteAtPosition(event->pos());
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

void HexDisplay::resizeEvent(QResizeEvent* event) {
    calculateLayout();
    QWidget::resizeEvent(event);
}

uint32_t HexDisplay::getByteAtPosition(const QPoint& pos) const {
    int hexStartX = addressColumnWidth_ + 10;
    
    if (pos.x() < hexStartX || pos.x() > hexStartX + hexColumnWidth_) {
        return 0xFFFFFFFF;
    }
    
    // Account for header row (starts at y=30)
    int row = (pos.y() - 30) / rowHeight_;
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
    int y = 30 + row * rowHeight_;  // Account for header row
    
    return QRect(x, y, charWidth_ * 2, charHeight_);
}

void HexDisplay::showByteEditDialog(uint32_t address) {
    showMultiByteEditDialog(address, 1);
}

void HexDisplay::showMultiByteEditDialog(uint32_t startAddress, uint32_t count) {
    if (!patches_ || startAddress >= flashData_.size()) {
        return;
    }
    
    // Limit count to available data
    if (startAddress + count > flashData_.size()) {
        count = flashData_.size() - startAddress;
    }
    
    // Get original data
    std::vector<uint8_t> originalData(flashData_.begin() + startAddress,
                                     flashData_.begin() + startAddress + count);
    
    // Get current data (apply any existing patches)
    std::vector<uint8_t> currentData = originalData;
    for (const auto& patch : *patches_) {
        if (!patch.enabled) continue;
        
        uint32_t patchStart = patch.address;
        uint32_t patchEnd = patch.address + static_cast<uint32_t>(patch.data.size());
        uint32_t rangeStart = startAddress;
        uint32_t rangeEnd = startAddress + count;
        
        // Check for overlap
        if (patchStart < rangeEnd && patchEnd > rangeStart) {
            uint32_t overlapStart = std::max(patchStart, rangeStart);
            uint32_t overlapEnd = std::min(patchEnd, rangeEnd);
            
            for (uint32_t addr = overlapStart; addr < overlapEnd; ++addr) {
                uint32_t patchOffset = addr - patchStart;
                uint32_t dataOffset = addr - rangeStart;
                if (patchOffset < patch.data.size() && dataOffset < currentData.size()) {
                    currentData[dataOffset] = patch.data[patchOffset];
                }
            }
        }
    }
    
    ByteEditDialog dialog(startAddress, originalData, currentData, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto newPatches = dialog.getOptimizedPatches();
        for (const auto& patch : newPatches) {
            emit patchCreated(patch);
        }
        update();
    }
}

uint8_t HexDisplay::getNextAvailablePatchId() const {
    if (!patches_) {
        return 0;
    }
    
    std::set<uint8_t> usedIds;
    for (const auto& patch : *patches_) {
        usedIds.insert(patch.id);
    }
    
    for (uint8_t id = 0; id < 16; ++id) {
        if (usedIds.find(id) == usedIds.end()) {
            return id;
        }
    }
    
    return 0;
}

// ============================================================================
// HexViewer Implementation
// ============================================================================

HexViewer::HexViewer(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void HexViewer::setupUi() {
    hexDisplay_ = new HexDisplay(this);
    
    editGotoAddress_ = new QLineEdit(this);
    editGotoAddress_->setPlaceholderText("Address (hex)");
    editGotoAddress_->setMaxLength(8);
    
    btnGoto_ = new QPushButton("Go To", this);
    connect(btnGoto_, &QPushButton::clicked, this, &HexViewer::onGotoClicked);
    
    editSearch_ = new QLineEdit(this);
    editSearch_->setPlaceholderText("Search (hex bytes)");
    
    btnSearch_ = new QPushButton("Search", this);
    connect(btnSearch_, &QPushButton::clicked, this, &HexViewer::onSearchClicked);
    
    btnClearHighlights_ = new QPushButton("Clear Highlights", this);
    connect(btnClearHighlights_, &QPushButton::clicked, this, &HexViewer::onClearHighlightsClicked);
    
    btnLoadFlash_ = new QPushButton("Load Flash...", this);
    connect(btnLoadFlash_, &QPushButton::clicked, this, &HexViewer::onLoadFlashClicked);
    
    lblStatus_ = new QLabel("No flash data loaded", this);
    
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel("Address:", this));
    controlLayout->addWidget(editGotoAddress_);
    controlLayout->addWidget(btnGoto_);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(new QLabel("Search:", this));
    controlLayout->addWidget(editSearch_);
    controlLayout->addWidget(btnSearch_);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(btnClearHighlights_);
    controlLayout->addStretch();
    controlLayout->addWidget(btnLoadFlash_);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(hexDisplay_, 1);
    mainLayout->addWidget(lblStatus_);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    setLayout(mainLayout);
    
    connect(hexDisplay_, &HexDisplay::patchCreated, this, &HexViewer::patchCreated);
    connect(hexDisplay_, &HexDisplay::applyPatchesRequested, this, &HexViewer::applyPatchesRequested);
}

bool HexViewer::loadFlashData(const std::string& filename) {
    if (hexDisplay_->loadFlashData(filename)) {
        lblStatus_->setText(QString("Loaded: %1").arg(QString::fromStdString(filename)));
        return true;
    }
    lblStatus_->setText("Failed to load flash data");
    return false;
}

void HexViewer::setPatches(std::vector<rebear::Patch>* patches) {
    hexDisplay_->setPatches(patches);
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

void HexViewer::onSearchClicked() {
    QMessageBox::information(this, "Search", "Search functionality not yet implemented");
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

// ============================================================================
// SIMPLIFIED ByteEditDialog - Much simpler, more robust
// ============================================================================

ByteEditDialog::ByteEditDialog(uint32_t startAddress,
                               const std::vector<uint8_t>& originalData,
                               const std::vector<uint8_t>& currentData,
                               QWidget* parent)
    : QDialog(parent)
    , startAddress_(startAddress)
    , originalData_(originalData)
{
    // Initialize with current values
    for (size_t i = 0; i < currentData.size() && i < originalData.size(); ++i) {
        if (currentData[i] != originalData[i]) {
            modifiedBytes_[static_cast<uint32_t>(i)] = currentData[i];
        }
    }
    
    setupUi();
    updatePreview();
}

void ByteEditDialog::setupUi() {
    setWindowTitle("Edit Firmware Bytes");
    setModal(true);
    setMinimumSize(600, 400);
    
    // Address info
    std::ostringstream oss;
    oss << "Address: 0x" << std::hex << std::setw(8) << std::setfill('0') 
        << std::uppercase << startAddress_ << " (" << std::dec << originalData_.size() << " bytes)";
    lblAddress_ = new QLabel(QString::fromStdString(oss.str()), this);
    
    // Table for byte editing
    tableBytes_ = new QTableWidget(this);
    tableBytes_->setColumnCount(3);
    tableBytes_->setHorizontalHeaderLabels({"Address", "Original", "New Value"});
    tableBytes_->horizontalHeader()->setStretchLastSection(true);
    tableBytes_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    
    // Populate table
    tableBytes_->setRowCount(static_cast<int>(originalData_.size()));
    for (size_t i = 0; i < originalData_.size(); ++i) {
        // Address
        oss.str("");
        oss << "0x" << std::hex << std::setw(8) << std::setfill('0') 
            << std::uppercase << (startAddress_ + static_cast<uint32_t>(i));
        QTableWidgetItem* itemAddr = new QTableWidgetItem(QString::fromStdString(oss.str()));
        itemAddr->setFlags(itemAddr->flags() & ~Qt::ItemIsEditable);
        tableBytes_->setItem(static_cast<int>(i), 0, itemAddr);
        
        // Original
        oss.str("");
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << std::uppercase << static_cast<int>(originalData_[i]);
        QTableWidgetItem* itemOrig = new QTableWidgetItem(QString::fromStdString(oss.str()));
        itemOrig->setFlags(itemOrig->flags() & ~Qt::ItemIsEditable);
        tableBytes_->setItem(static_cast<int>(i), 1, itemOrig);
        
        // New value (editable)
        QString newValText;
        if (modifiedBytes_.find(static_cast<uint32_t>(i)) != modifiedBytes_.end()) {
            oss.str("");
            oss << std::hex << std::setw(2) << std::setfill('0') 
                << std::uppercase << static_cast<int>(modifiedBytes_[static_cast<uint32_t>(i)]);
            newValText = QString::fromStdString(oss.str());
        }
        tableBytes_->setItem(static_cast<int>(i), 2, new QTableWidgetItem(newValText));
    }
    
    connect(tableBytes_, &QTableWidget::itemChanged, this, &ByteEditDialog::onByteValueChanged);
    
    // Preview
    txtPreview_ = new QTextEdit(this);
    txtPreview_->setReadOnly(true);
    txtPreview_->setMaximumHeight(120);
    
    // Buttons
    btnApply_ = new QPushButton("Apply", this);
    btnCancel_ = new QPushButton("Cancel", this);
    
    connect(btnApply_, &QPushButton::clicked, this, &ByteEditDialog::onApplyClicked);
    connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);
    
    // Layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(lblAddress_);
    mainLayout->addWidget(tableBytes_, 1);
    mainLayout->addWidget(new QLabel("Patch Preview:", this));
    mainLayout->addWidget(txtPreview_);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnApply_);
    buttonLayout->addWidget(btnCancel_);
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
}

std::map<uint32_t, uint8_t> ByteEditDialog::getModifiedBytes() const {
    return modifiedBytes_;
}

std::vector<rebear::Patch> ByteEditDialog::getOptimizedPatches() const {
    return calculateOptimalPatchRanges();
}

std::vector<rebear::Patch> ByteEditDialog::calculateOptimalPatchRanges() const {
    if (modifiedBytes_.empty()) {
        return {};
    }
    
    std::vector<rebear::Patch> patches;
    std::vector<uint32_t> sortedOffsets;
    
    for (const auto& pair : modifiedBytes_) {
        sortedOffsets.push_back(pair.first);
    }
    std::sort(sortedOffsets.begin(), sortedOffsets.end());
    
    // Create patches - group consecutive bytes, split on gaps > 4 bytes
    const uint32_t MAX_GAP = 4;
    
    uint32_t patchStart = sortedOffsets[0];
    uint32_t patchEnd = sortedOffsets[0] + 1;
    
    for (size_t i = 1; i < sortedOffsets.size(); ++i) {
        uint32_t offset = sortedOffsets[i];
        uint32_t gap = offset - patchEnd;
        
        if (gap <= MAX_GAP && (offset - patchStart) < 256) {
            patchEnd = offset + 1;
        } else {
            // Emit patch
            rebear::Patch patch;
            patch.address = startAddress_ + patchStart;
            patch.enabled = true;
            patch.id = 0; // Will be assigned by patch manager
            
            for (uint32_t off = patchStart; off < patchEnd; ++off) {
                auto it = modifiedBytes_.find(off);
                if (it != modifiedBytes_.end()) {
                    patch.data.push_back(it->second);
                } else if (off < originalData_.size()) {
                    patch.data.push_back(originalData_[off]);
                } else {
                    patch.data.push_back(0xFF);
                }
            }
            
            patches.push_back(patch);
            patchStart = offset;
            patchEnd = offset + 1;
        }
    }
    
    // Emit final patch
    rebear::Patch patch;
    patch.address = startAddress_ + patchStart;
    patch.enabled = true;
    patch.id = 0;
    
    for (uint32_t off = patchStart; off < patchEnd; ++off) {
        auto it = modifiedBytes_.find(off);
        if (it != modifiedBytes_.end()) {
            patch.data.push_back(it->second);
        } else if (off < originalData_.size()) {
            patch.data.push_back(originalData_[off]);
        } else {
            patch.data.push_back(0xFF);
        }
    }
    
    patches.push_back(patch);
    return patches;
}

void ByteEditDialog::updatePreview() {
    auto patches = calculateOptimalPatchRanges();
    
    std::ostringstream oss;
    oss << "Modified " << modifiedBytes_.size() << " byte(s)\n";
    oss << "Will create " << patches.size() << " patch(es):\n\n";
    
    for (size_t i = 0; i < patches.size(); ++i) {
        oss << "Patch " << (i+1) << ": 0x" << std::hex << std::setw(8) 
            << std::setfill('0') << std::uppercase << patches[i].address 
            << " (" << std::dec << patches[i].data.size() << " bytes)\n";
    }
    
    txtPreview_->setText(QString::fromStdString(oss.str()));
}

void ByteEditDialog::onByteValueChanged() {
    // Rebuild modifiedBytes_ from table
    modifiedBytes_.clear();
    
    for (int row = 0; row < tableBytes_->rowCount(); ++row) {
        QTableWidgetItem* item = tableBytes_->item(row, 2);
        if (!item || item->text().isEmpty()) {
            continue;
        }
        
        bool ok;
        uint8_t newValue = static_cast<uint8_t>(item->text().toUInt(&ok, 16));
        if (!ok) {
            continue;
        }
        
        // Only add if different from original
        if (static_cast<size_t>(row) < originalData_.size()) {
            if (newValue != originalData_[row]) {
                modifiedBytes_[static_cast<uint32_t>(row)] = newValue;
            }
        }
    }
    
    updatePreview();
}

void ByteEditDialog::onApplyClicked() {
    if (modifiedBytes_.empty()) {
        QMessageBox::warning(this, "No Changes", "No bytes have been modified.");
        return;
    }
    
    accept();
}

void ByteEditDialog::onAddByteClicked() {
    // Not implemented in simplified version
}

void ByteEditDialog::onRemoveByteClicked() {
    QList<QTableWidgetItem*> selected = tableBytes_->selectedItems();
    for (auto* item : selected) {
        if (item->column() == 2) {
            item->setText("");
        }
    }
    onByteValueChanged();
}

void ByteEditDialog::onOptimizeClicked() {
    updatePreview();
}

} // namespace gui
} // namespace rebear
