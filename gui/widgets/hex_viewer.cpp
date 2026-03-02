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
#include <fstream>
#include <iomanip>
#include <sstream>

namespace rebear {
namespace gui {

// ============================================================================
// HexDisplay Implementation
// ============================================================================

HexDisplay::HexDisplay(QWidget* parent)
    : QWidget(parent)
    , patchManager_(nullptr)
    , scrollOffset_(0)
    , bytesPerRow_(16)
    , visibleRows_(0)
    , hoveredByte_(0xFFFFFFFF)
    , isHovering_(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // Use monospace font
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
    
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    flashData_.resize(size);
    file.read(reinterpret_cast<char*>(flashData_.data()), size);
    
    scrollOffset_ = 0;
    update();
    return true;
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
    h.color = QColor(255, 255, 0, 100);  // Yellow with transparency
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
    
    // Address column: "00000000: "
    addressColumnWidth_ = charWidth_ * 10;
    
    // Hex column: "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  "
    hexColumnWidth_ = charWidth_ * (bytesPerRow_ * 3 + 2);
    
    // ASCII column: "|0123456789ABCDEF|"
    asciiColumnWidth_ = charWidth_ * (bytesPerRow_ + 2);
    
    visibleRows_ = (height() - 20) / rowHeight_;
}

void HexDisplay::paintEvent(QPaintEvent* /* event */) {
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
    
    // Get active patches
    std::map<uint32_t, rebear::Patch> patchMap;
    if (patchManager_) {
        auto patches = patchManager_->getPatches();
        for (const auto& patch : patches) {
            if (patch.enabled) {
                for (size_t i = 0; i < 8; ++i) {
                    patchMap[patch.address + i] = patch;
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
            
            // Check if this byte is highlighted
            // Check if this byte is highlighted
            for (const auto& h : highlights_) {
                if (address >= h.address && address < h.address + h.count) {
                    painter.fillRect(byteRect, h.color);
                    break;
                }
            }
            
            // Check if this byte is patched
            bool isPatched = patchMap.find(address) != patchMap.end();
            if (isPatched) {
                painter.fillRect(byteRect, QColor(255, 200, 200));  // Light red for patches
            }
            
            // Check if hovered
            if (isHovering_ && address == hoveredByte_) {
                painter.fillRect(byteRect, QColor(200, 200, 255));  // Light blue for hover
            }
            
            // Draw byte value
            uint8_t byte = flashData_[address];
            if (isPatched) {
                // Show patched value
                auto it = patchMap.find(address);
                const auto& patch = it->second;
                size_t offset = address - patch.address;
                byte = patch.data[offset];
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
            
            uint8_t byte = flashData_[address];
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
    
    scrollOffset_ = newOffset;
    update();
}

void HexDisplay::resizeEvent(QResizeEvent* event) {
    calculateLayout();
    QWidget::resizeEvent(event);
}

uint32_t HexDisplay::getByteAtPosition(const QPoint& pos) const {
    int hexStartX = addressColumnWidth_ + 10;
    
    // Check if in hex column
    if (pos.x() < hexStartX || pos.x() > hexStartX + hexColumnWidth_) {
        return 0xFFFFFFFF;
    }
    
    // Calculate row
    int row = (pos.y() - 10) / rowHeight_;
    if (row < 0 || row >= static_cast<int>(visibleRows_)) {
        return 0xFFFFFFFF;
    }
    
    // Calculate column
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

void HexDisplay::showByteEditDialog(uint32_t address) {
    if (!patchManager_) {
        QMessageBox::warning(this, "Error", "No patch manager available");
        return;
    }
    
    uint8_t originalValue = flashData_[address];
    uint8_t currentValue = originalValue;
    
    // Check if already patched
    auto patches = patchManager_->getPatches();
    for (const auto& patch : patches) {
        if (address >= patch.address && address < patch.address + 8) {
            size_t offset = address - patch.address;
            currentValue = patch.data[offset];
            break;
        }
    }
    
    uint8_t patchId = getNextAvailablePatchId();
    
    ByteEditDialog dialog(address, currentValue, originalValue, patchId, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Create patch
        rebear::Patch patch;
        patch.id = dialog.getPatchId();
        patch.address = address;
        patch.data = dialog.getPatchData();
        patch.enabled = true;
        
        emit patchCreated(patch);
        update();
    }
}

uint8_t HexDisplay::getNextAvailablePatchId() const {
    if (!patchManager_) {
        return 0;
    }
    
    std::set<uint8_t> usedIds;
    auto patches = patchManager_->getPatches();
    for (const auto& patch : patches) {
        usedIds.insert(patch.id);
    }
    
    for (uint8_t id = 0; id < 16; ++id) {
        if (usedIds.find(id) == usedIds.end()) {
            return id;
        }
    }
    
    return 0;  // All IDs used, will need to replace
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
    // Create hex display
    hexDisplay_ = new HexDisplay(this);
    
    // Create controls
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
    
    // Layout controls
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
    
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(hexDisplay_, 1);
    mainLayout->addWidget(lblStatus_);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    setLayout(mainLayout);
    
    // Connect signals
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

void HexViewer::onSearchClicked() {
    // TODO: Implement search functionality
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
// ByteEditDialog Implementation
// ============================================================================

ByteEditDialog::ByteEditDialog(uint32_t address, uint8_t currentValue,
                               uint8_t originalValue, uint8_t patchId,
                               QWidget* parent)
    : QDialog(parent)
    , address_(address)
    , originalValue_(originalValue)
    , patchId_(patchId)
{
    setupUi();
    
    // Set initial value
    std::ostringstream oss;
    oss << std::hex << std::setw(2) << std::setfill('0') 
        << std::uppercase << static_cast<int>(currentValue);
    editByte0_->setText(QString::fromStdString(oss.str()));
}

void ByteEditDialog::setupUi() {
    setWindowTitle("Edit Byte / Create Patch");
    setModal(true);
    
    // Address info
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') 
        << std::uppercase << address_;
    lblAddress_ = new QLabel(QString::fromStdString(oss.str()), this);
    
    oss.str("");
    oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
        << std::uppercase << static_cast<int>(originalValue_);
    lblOriginal_ = new QLabel(QString::fromStdString(oss.str()), this);
    
    // Patch ID
    spinPatchId_ = new QSpinBox(this);
    spinPatchId_->setRange(0, 15);
    spinPatchId_->setValue(patchId_);
    
    // Byte editors (8 bytes for patch)
    editByte0_ = new QLineEdit(this);
    editByte1_ = new QLineEdit(this);
    editByte2_ = new QLineEdit(this);
    editByte3_ = new QLineEdit(this);
    editByte4_ = new QLineEdit(this);
    editByte5_ = new QLineEdit(this);
    editByte6_ = new QLineEdit(this);
    editByte7_ = new QLineEdit(this);
    
    byteEdits_ = {editByte0_, editByte1_, editByte2_, editByte3_,
                  editByte4_, editByte5_, editByte6_, editByte7_};
    
    for (auto* edit : byteEdits_) {
        edit->setMaxLength(2);
        edit->setFixedWidth(40);
        connect(edit, &QLineEdit::textChanged, this, &ByteEditDialog::onByteValueChanged);
    }
    
    // Preview
    txtPreview_ = new QTextEdit(this);
    txtPreview_->setReadOnly(true);
    txtPreview_->setMaximumHeight(100);
    
    // Buttons
    btnApply_ = new QPushButton("Apply Patch", this);
    btnCancel_ = new QPushButton("Cancel", this);
    connect(btnApply_, &QPushButton::clicked, this, &ByteEditDialog::onApplyClicked);
    connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);
    
    // Layout
    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow("Address:", lblAddress_);
    formLayout->addRow("Original Value:", lblOriginal_);
    formLayout->addRow("Patch ID (0-15):", spinPatchId_);
    
    QHBoxLayout* byteLayout = new QHBoxLayout();
    byteLayout->addWidget(new QLabel("Patch Data (8 bytes):", this));
    for (auto* edit : byteEdits_) {
        byteLayout->addWidget(edit);
    }
    byteLayout->addStretch();
    formLayout->addRow(byteLayout);
    
    formLayout->addRow("Preview:", txtPreview_);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnApply_);
    buttonLayout->addWidget(btnCancel_);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
    
    onByteValueChanged();
}

std::array<uint8_t, 8> ByteEditDialog::getPatchData() const {
    std::array<uint8_t, 8> data;
    for (size_t i = 0; i < 8; ++i) {
        bool ok;
        data[i] = byteEdits_[i]->text().toUInt(&ok, 16);
        if (!ok) data[i] = 0;
    }
    return data;
}

void ByteEditDialog::onByteValueChanged() {
    // Update preview
    QString preview = "Patch will replace 8 bytes starting at address:\n";
    
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') 
        << std::uppercase << address_ << ": ";
    
    for (size_t i = 0; i < 8; ++i) {
        QString text = byteEdits_[i]->text().toUpper();
        if (text.length() == 2) {
            oss << text.toStdString() << " ";
        } else {
            oss << "?? ";
        }
    }
    
    preview += QString::fromStdString(oss.str());
    txtPreview_->setText(preview);
}

void ByteEditDialog::onApplyClicked() {
    // Validate all bytes
    for (size_t i = 0; i < 8; ++i) {
        if (byteEdits_[i]->text().length() != 2) {
            QMessageBox::warning(this, "Invalid Data", 
                QString("Byte %1 must be exactly 2 hex characters").arg(i));
            return;
        }
    }
    
    accept();
}

} // namespace gui
} // namespace rebear
