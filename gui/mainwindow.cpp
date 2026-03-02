#include "mainwindow.h"
#include "widgets/transaction_viewer.h"
#include "widgets/patch_editor.h"
#include "widgets/hex_viewer.h"
#include "widgets/connection_dialog.h"
#include "rebear/spi_protocol.h"
#include "rebear/spi_protocol_network.h"
#include "rebear/patch_manager.h"
#include "rebear/gpio_control.h"
#include "rebear/gpio_control_network.h"
#include "rebear/transaction.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isConnected_(false)
    , transactionCount_(0)
    , currentDevice_("/dev/spidev0.0")
    , currentSpeed_(100000)
    , useNetwork_(false)
    , remoteHost_("raspberrypi.local")
    , remotePort_(9876)
{
    // Set window properties
    setWindowTitle("Rebear - Teddy Bear Reverse Engineering");
    resize(1200, 800);
    
    // Initialize patch manager (always needed)
    patchManager_ = std::make_unique<rebear::PatchManager>();
    
    // Create polling timer
    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::onPollTransactions);
    
    // Create UI components
    createActions();
    createMenus();
    createToolBar();
    createStatusBar();
    createCentralWidget();
    
    // Load connection settings
    loadConnectionSettings();
    
    // Initial state
    updateConnectionState(false);
    updateStatusBar("Ready");
    logMessage("Application started");
}

MainWindow::~MainWindow()
{
    // Stop polling
    if (pollTimer_->isActive()) {
        pollTimer_->stop();
    }
    
    // Disconnect if connected
    if (isConnected_) {
        if (useNetwork_) {
            if (spiNetwork_) spiNetwork_->close();
        } else {
            if (spi_) spi_->close();
        }
    }
}

void MainWindow::createActions()
{
    // File menu actions
    connectAction_ = new QAction(tr("&Connect"), this);
    connectAction_->setShortcut(QKeySequence(tr("Ctrl+O")));
    connectAction_->setStatusTip(tr("Connect to FPGA"));
    connect(connectAction_, &QAction::triggered, this, &MainWindow::onConnect);
    
    disconnectAction_ = new QAction(tr("&Disconnect"), this);
    disconnectAction_->setShortcut(QKeySequence(tr("Ctrl+D")));
    disconnectAction_->setStatusTip(tr("Disconnect from FPGA"));
    connect(disconnectAction_, &QAction::triggered, this, &MainWindow::onDisconnect);
    
    exportAction_ = new QAction(tr("&Export..."), this);
    exportAction_->setShortcut(QKeySequence(tr("Ctrl+E")));
    exportAction_->setStatusTip(tr("Export transaction log"));
    connect(exportAction_, &QAction::triggered, this, &MainWindow::onExport);
    
    exitAction_ = new QAction(tr("E&xit"), this);
    exitAction_->setShortcut(QKeySequence(tr("Ctrl+Q")));
    exitAction_->setStatusTip(tr("Exit application"));
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);
    
    // Edit menu actions
    clearTransactionsAction_ = new QAction(tr("Clear &Transactions"), this);
    clearTransactionsAction_->setShortcut(QKeySequence(tr("Ctrl+T")));
    clearTransactionsAction_->setStatusTip(tr("Clear transaction buffer"));
    connect(clearTransactionsAction_, &QAction::triggered, this, &MainWindow::onClearTransactions);
    
    settingsAction_ = new QAction(tr("&Settings..."), this);
    settingsAction_->setShortcut(QKeySequence(tr("Ctrl+,")));
    settingsAction_->setStatusTip(tr("Configure application settings"));
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::onSettings);
    
    // Tools menu actions
    loadPatchesAction_ = new QAction(tr("&Load Patches..."), this);
    loadPatchesAction_->setStatusTip(tr("Load patches from file"));
    connect(loadPatchesAction_, &QAction::triggered, this, &MainWindow::onLoadPatches);
    
    savePatchesAction_ = new QAction(tr("&Save Patches..."), this);
    savePatchesAction_->setStatusTip(tr("Save patches to file"));
    connect(savePatchesAction_, &QAction::triggered, this, &MainWindow::onSavePatches);
    
    clearPatchesAction_ = new QAction(tr("&Clear Patches"), this);
    clearPatchesAction_->setStatusTip(tr("Clear all patches"));
    connect(clearPatchesAction_, &QAction::triggered, this, &MainWindow::onClearPatches);
    
    buttonPressAction_ = new QAction(tr("Button &Press"), this);
    buttonPressAction_->setStatusTip(tr("Press teddy bear button"));
    connect(buttonPressAction_, &QAction::triggered, this, &MainWindow::onButtonPress);
    
    buttonReleaseAction_ = new QAction(tr("Button &Release"), this);
    buttonReleaseAction_->setStatusTip(tr("Release teddy bear button"));
    connect(buttonReleaseAction_, &QAction::triggered, this, &MainWindow::onButtonRelease);
    
    buttonClickAction_ = new QAction(tr("Button &Click"), this);
    buttonClickAction_->setShortcut(QKeySequence(tr("Ctrl+B")));
    buttonClickAction_->setStatusTip(tr("Click teddy bear button"));
    connect(buttonClickAction_, &QAction::triggered, this, &MainWindow::onButtonClick);
    
    // Help menu actions
    aboutAction_ = new QAction(tr("&About"), this);
    aboutAction_->setStatusTip(tr("About Rebear"));
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createMenus()
{
    // File menu
    fileMenu_ = menuBar()->addMenu(tr("&File"));
    fileMenu_->addAction(connectAction_);
    fileMenu_->addAction(disconnectAction_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(exportAction_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(exitAction_);
    
    // Edit menu
    editMenu_ = menuBar()->addMenu(tr("&Edit"));
    editMenu_->addAction(clearTransactionsAction_);
    editMenu_->addSeparator();
    editMenu_->addAction(settingsAction_);
    
    // View menu (placeholder for future widgets)
    viewMenu_ = menuBar()->addMenu(tr("&View"));
    
    // Tools menu
    toolsMenu_ = menuBar()->addMenu(tr("&Tools"));
    toolsMenu_->addAction(loadPatchesAction_);
    toolsMenu_->addAction(savePatchesAction_);
    toolsMenu_->addAction(clearPatchesAction_);
    toolsMenu_->addSeparator();
    toolsMenu_->addAction(buttonPressAction_);
    toolsMenu_->addAction(buttonReleaseAction_);
    toolsMenu_->addAction(buttonClickAction_);
    
    // Help menu
    helpMenu_ = menuBar()->addMenu(tr("&Help"));
    helpMenu_->addAction(aboutAction_);
}

void MainWindow::createToolBar()
{
    mainToolBar_ = addToolBar(tr("Main Toolbar"));
    mainToolBar_->setMovable(false);
    
    mainToolBar_->addAction(connectAction_);
    mainToolBar_->addAction(disconnectAction_);
    mainToolBar_->addSeparator();
    mainToolBar_->addAction(clearTransactionsAction_);
    mainToolBar_->addSeparator();
    mainToolBar_->addAction(buttonClickAction_);
    mainToolBar_->addSeparator();
    mainToolBar_->addAction(exportAction_);
}

void MainWindow::createStatusBar()
{
    // Connection status
    connectionStatusLabel_ = new QLabel("Disconnected");
    connectionStatusLabel_->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusBar()->addPermanentWidget(connectionStatusLabel_);
    
    // Transaction count
    transactionCountLabel_ = new QLabel("Transactions: 0");
    transactionCountLabel_->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusBar()->addPermanentWidget(transactionCountLabel_);
    
    // Buffer status
    bufferStatusLabel_ = new QLabel("Buffer: Unknown");
    bufferStatusLabel_->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusBar()->addPermanentWidget(bufferStatusLabel_);
    
    statusBar()->showMessage("Ready");
}

void MainWindow::createCentralWidget()
{
    // Create main vertical splitter
    mainSplitter_ = new QSplitter(Qt::Vertical, this);
    
    // Create top area with transaction viewer, patch editor, and hex viewer
    QWidget* topWidget = new QWidget(this);
    QHBoxLayout* topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create left side (transaction viewer and patch editor)
    QWidget* leftWidget = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create transaction viewer
    transactionViewer_ = new rebear::gui::TransactionViewer(this);
    
    // Create patch editor
    patchEditor_ = new rebear::gui::PatchEditor(this);
    patchEditor_->setPatchManager(patchManager_.get());
    
    // Add to left layout (50/50 split vertically)
    leftLayout->addWidget(transactionViewer_);
    leftLayout->addWidget(patchEditor_);
    
    // Create hex viewer
    hexViewer_ = new rebear::gui::HexViewer(this);
    hexViewer_->setPatchManager(patchManager_.get());
    
    // Add to horizontal layout (40% left, 60% hex viewer)
    topLayout->addWidget(leftWidget, 2);
    topLayout->addWidget(hexViewer_, 3);
    
    // Create log widget
    logWidget_ = new QTextEdit(this);
    logWidget_->setReadOnly(true);
    logWidget_->setMaximumHeight(200);
    
    // Add widgets to main splitter
    mainSplitter_->addWidget(topWidget);
    mainSplitter_->addWidget(logWidget_);
    
    // Set splitter sizes (80% top, 20% bottom)
    mainSplitter_->setStretchFactor(0, 4);
    mainSplitter_->setStretchFactor(1, 1);
    
    // Connect signals
    connect(this, &MainWindow::transactionReceived,
            transactionViewer_, &rebear::gui::TransactionViewer::addTransaction);
    
    // Connect transaction viewer to hex viewer
    connect(transactionViewer_, &rebear::gui::TransactionViewer::transactionClicked,
            hexViewer_, &rebear::gui::HexViewer::onTransactionClicked);
    
    // Connect hex viewer patch creation to patch manager
    connect(hexViewer_, &rebear::gui::HexViewer::patchCreated,
            this, [this](const rebear::Patch& patch) {
                if (patchManager_->addPatch(patch)) {
                    patchEditor_->refresh();
                    hexViewer_->refresh();
                    logMessage(QString("Created patch ID %1 at address 0x%2")
                              .arg(patch.id)
                              .arg(patch.address, 6, 16, QChar('0')).toUpper());
                } else {
                    QMessageBox::warning(this, "Patch Error",
                        QString::fromStdString(patchManager_->getLastError()));
                }
            });
    
    connect(patchEditor_, &rebear::gui::PatchEditor::applyAllRequested,
            this, [this]() {
                if (!isConnected_) return;
                bool success = false;
                if (useNetwork_) {
                    success = patchManager_->applyAll(*spiNetwork_);
                } else {
                    success = patchManager_->applyAll(*spi_);
                }
                if (success) {
                    updateStatusBar("All patches applied");
                    logMessage("All patches applied to FPGA");
                    hexViewer_->refresh();
                } else {
                    QMessageBox::warning(this, "Apply Failed",
                        QString::fromStdString(patchManager_->getLastError()));
                }
            });
    
    connect(patchEditor_, &rebear::gui::PatchEditor::clearAllRequested,
            this, &MainWindow::onClearPatches);
    
    connect(patchEditor_, &rebear::gui::PatchEditor::patchesChanged,
            hexViewer_, &rebear::gui::HexViewer::refresh);
    
    // Auto-load flash.bin if it exists
    if (hexViewer_->loadFlashData("data/flash.bin")) {
        transactionViewer_->loadFlashData("data/flash.bin");
        logMessage("Loaded flash.bin successfully");
    } else {
        logMessage("Warning: Could not load data/flash.bin");
    }
    
    setCentralWidget(mainSplitter_);
}

void MainWindow::updateConnectionState(bool connected)
{
    isConnected_ = connected;
    
    // Update actions
    connectAction_->setEnabled(!connected);
    disconnectAction_->setEnabled(connected);
    clearTransactionsAction_->setEnabled(connected);
    loadPatchesAction_->setEnabled(connected);
    savePatchesAction_->setEnabled(connected);
    clearPatchesAction_->setEnabled(connected);
    buttonPressAction_->setEnabled(connected);
    buttonReleaseAction_->setEnabled(connected);
    buttonClickAction_->setEnabled(connected);
    exportAction_->setEnabled(connected);
    
    // Update status label
    if (connected) {
        QString connStr = useNetwork_
            ? QString("Network: %1:%2").arg(remoteHost_).arg(remotePort_)
            : QString("Local: %1").arg(currentDevice_);
        connectionStatusLabel_->setText(connStr);
        connectionStatusLabel_->setStyleSheet("QLabel { background-color: #90EE90; }");
    } else {
        connectionStatusLabel_->setText("Disconnected");
        connectionStatusLabel_->setStyleSheet("QLabel { background-color: #FFB6C1; }");
    }
}

void MainWindow::updateStatusBar(const QString& message)
{
    statusBar()->showMessage(message, 3000);
}

void MainWindow::logMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    logWidget_->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::onConnect()
{
    // Show connection dialog
    rebear::ConnectionDialog dialog(this);
    dialog.setMode(useNetwork_ ? rebear::ConnectionDialog::Mode::Network
                               : rebear::ConnectionDialog::Mode::Local);
    dialog.setHostname(remoteHost_);
    dialog.setPort(remotePort_);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get connection settings
    useNetwork_ = (dialog.getMode() == rebear::ConnectionDialog::Mode::Network);
    
    if (useNetwork_) {
        remoteHost_ = dialog.getHostname();
        remotePort_ = dialog.getPort();
        
        logMessage(QString("Connecting to remote server %1:%2...")
                  .arg(remoteHost_).arg(remotePort_));
        
        // Create network objects
        spiNetwork_ = std::make_unique<rebear::SPIProtocolNetwork>(
            remoteHost_.toStdString(), remotePort_);
        
        if (!spiNetwork_->open(currentDevice_.toStdString(), currentSpeed_)) {
            QMessageBox::critical(this, "Connection Error",
                QString("Failed to connect to remote server:\n%1")
                .arg(QString::fromStdString(spiNetwork_->getLastError())));
            logMessage(QString("Connection failed: %1")
                      .arg(QString::fromStdString(spiNetwork_->getLastError())));
            spiNetwork_.reset();
            return;
        }
        
        logMessage("Successfully connected to remote server");
        
        // Initialize GPIO via network
        buttonNetwork_ = std::make_unique<rebear::GPIOControlNetwork>(
            3, rebear::GPIOControl::Direction::Output,
            remoteHost_.toStdString(), remotePort_);
        
        if (buttonNetwork_->init()) {
            logMessage("Button control initialized (GPIO 3 via network)");
        } else {
            logMessage(QString("Warning: Button control failed: %1")
                      .arg(QString::fromStdString(buttonNetwork_->getLastError())));
        }
        
        bufferMonitorNetwork_ = std::make_unique<rebear::GPIOControlNetwork>(
            4, rebear::GPIOControl::Direction::Input,
            remoteHost_.toStdString(), remotePort_);
        
        if (bufferMonitorNetwork_->init()) {
            logMessage("Buffer monitor initialized (GPIO 4 via network)");
        } else {
            logMessage(QString("Warning: Buffer monitor failed: %1")
                      .arg(QString::fromStdString(bufferMonitorNetwork_->getLastError())));
        }
        
    } else {
        // Local mode
        logMessage(QString("Connecting to local hardware %1 at %2 Hz...")
                  .arg(currentDevice_).arg(currentSpeed_));
        
        spi_ = std::make_unique<rebear::SPIProtocol>();
        
        if (!spi_->open(currentDevice_.toStdString(), currentSpeed_)) {
            QMessageBox::critical(this, "Connection Error",
                QString("Failed to connect to FPGA:\n%1")
                .arg(QString::fromStdString(spi_->getLastError())));
            logMessage(QString("Connection failed: %1")
                      .arg(QString::fromStdString(spi_->getLastError())));
            spi_.reset();
            return;
        }
        
        logMessage("Successfully connected to FPGA");
        
        // Initialize GPIO
        buttonControl_ = std::make_unique<rebear::ButtonControl>(3);
        if (buttonControl_->init()) {
            logMessage("Button control initialized (GPIO 3)");
        } else {
            logMessage(QString("Warning: Button control failed: %1")
                      .arg(QString::fromStdString(buttonControl_->getLastError())));
        }
        
        bufferMonitor_ = std::make_unique<rebear::BufferReadyMonitor>(4);
        if (bufferMonitor_->init()) {
            logMessage("Buffer monitor initialized (GPIO 4)");
        } else {
            logMessage(QString("Warning: Buffer monitor failed: %1")
                      .arg(QString::fromStdString(bufferMonitor_->getLastError())));
        }
    }
    
    // Save settings if requested
    if (dialog.shouldRemember()) {
        saveConnectionSettings();
    }
    
    // Start polling timer (100ms interval)
    pollTimer_->start(100);
    
    updateConnectionState(true);
    updateStatusBar("Connected");
    emit connected();
}

void MainWindow::onDisconnect()
{
    // Stop polling
    pollTimer_->stop();
    
    // Close connections
    if (useNetwork_) {
        if (spiNetwork_) {
            spiNetwork_->close();
            spiNetwork_.reset();
        }
        if (buttonNetwork_) {
            buttonNetwork_->close();
            buttonNetwork_.reset();
        }
        if (bufferMonitorNetwork_) {
            bufferMonitorNetwork_->close();
            bufferMonitorNetwork_.reset();
        }
        logMessage("Disconnected from remote server");
    } else {
        if (spi_) {
            spi_->close();
            spi_.reset();
        }
        // ButtonControl and BufferReadyMonitor don't have close() - destructor handles cleanup
        buttonControl_.reset();
        bufferMonitor_.reset();
        logMessage("Disconnected from FPGA");
    }
    
    updateConnectionState(false);
    updateStatusBar("Disconnected");
    
    emit disconnected();
}

void MainWindow::onClearTransactions()
{
    if (!isConnected_) return;
    
    bool success = false;
    std::string error;
    
    if (useNetwork_) {
        success = spiNetwork_->clearTransactions();
        error = spiNetwork_->getLastError();
    } else {
        success = spi_->clearTransactions();
        error = spi_->getLastError();
    }
    
    if (success) {
        transactionCount_ = 0;
        transactionCountLabel_->setText("Transactions: 0");
        transactionViewer_->clear();
        updateStatusBar("Transaction buffer cleared");
        logMessage("Transaction buffer cleared");
    } else {
        QMessageBox::warning(this, "Clear Failed",
                           QString("Failed to clear transactions:\n%1")
                           .arg(QString::fromStdString(error)));
        logMessage(QString("Clear failed: %1")
                  .arg(QString::fromStdString(error)));
    }
}

void MainWindow::onPollTransactions()
{
    if (!isConnected_) return;
    
    // Check buffer ready
    bool ready = false;
    if (useNetwork_) {
        if (bufferMonitorNetwork_) {
            ready = bufferMonitorNetwork_->read();
        }
    } else {
        if (bufferMonitor_) {
            ready = bufferMonitor_->isReady();
        }
    }
    
    if (!ready) {
        return;
    }
    
    // Read ALL available transactions in the buffer
    // Keep reading while buffer has data
    while (true) {
        std::optional<rebear::Transaction> trans;
        
        if (useNetwork_) {
            trans = spiNetwork_->readTransaction();
        } else {
            trans = spi_->readTransaction();
        }
        
        if (trans && trans->address != 0xFFFFFF) {
            transactionCount_++;
            transactionCountLabel_->setText(QString("Transactions: %1").arg(transactionCount_));
            
            // Log transaction
            QString countStr = (trans->count == 0xFFFFFF) ? "PATCHED" : QString::number(trans->count);
            logMessage(QString("Transaction: Addr=0x%1 Count=%2 Time=%3ms")
                      .arg(trans->address, 6, 16, QChar('0'))
                      .arg(countStr)
                      .arg(trans->timestamp));
            
            emit transactionReceived(*trans);
        } else {
            // No more valid transactions, exit loop
            break;
        }
    }
}

void MainWindow::onLoadPatches()
{
    if (!isConnected_) return;
    
    QString filename = QFileDialog::getOpenFileName(this,
                                                   tr("Load Patches"),
                                                   "",
                                                   tr("JSON Files (*.json);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        if (patchManager_->loadFromFile(filename.toStdString())) {
            // Refresh patch editor
            patchEditor_->refresh();
            
            // Apply all patches to FPGA
            if (patchManager_->applyAll(*spi_)) {
                updateStatusBar(QString("Loaded %1 patches").arg(patchManager_->count()));
                logMessage(QString("Loaded and applied %1 patches from %2")
                          .arg(patchManager_->count())
                          .arg(filename));
            } else {
                QMessageBox::warning(this, "Apply Failed",
                                   QString("Patches loaded but failed to apply:\n%1")
                                   .arg(QString::fromStdString(patchManager_->getLastError())));
            }
        } else {
            QMessageBox::critical(this, "Load Failed",
                                QString("Failed to load patches:\n%1")
                                .arg(QString::fromStdString(patchManager_->getLastError())));
        }
    }
}

void MainWindow::onSavePatches()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                   tr("Save Patches"),
                                                   "",
                                                   tr("JSON Files (*.json);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        if (patchManager_->saveToFile(filename.toStdString())) {
            updateStatusBar("Patches saved");
            logMessage(QString("Saved %1 patches to %2")
                      .arg(patchManager_->count())
                      .arg(filename));
        } else {
            QMessageBox::critical(this, "Save Failed",
                                QString("Failed to save patches:\n%1")
                                .arg(QString::fromStdString(patchManager_->getLastError())));
        }
    }
}

void MainWindow::onClearPatches()
{
    if (!isConnected_) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                             "Clear Patches",
                                                             "Clear all patches from FPGA?",
                                                             QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (patchManager_->clearAll(*spi_)) {
            updateStatusBar("All patches cleared");
            logMessage("All patches cleared from FPGA");
        } else {
            QMessageBox::warning(this, "Clear Failed",
                               QString("Failed to clear patches:\n%1")
                               .arg(QString::fromStdString(patchManager_->getLastError())));
        }
    }
}

void MainWindow::onButtonPress()
{
    if (!isConnected_) return;
    
    bool success = false;
    std::string error;
    
    if (useNetwork_) {
        if (buttonNetwork_) {
            success = buttonNetwork_->write(true);
            error = buttonNetwork_->getLastError();
        }
    } else {
        if (buttonControl_) {
            success = buttonControl_->press();
            error = buttonControl_->getLastError();
        }
    }
    
    if (success) {
        updateStatusBar("Button pressed");
        logMessage("Button pressed (GPIO 3 HIGH)");
        emit buttonPressed();
    } else {
        QMessageBox::warning(this, "Button Control Failed",
                           QString("Failed to press button:\n%1")
                           .arg(QString::fromStdString(error)));
    }
}

void MainWindow::onButtonRelease()
{
    if (!isConnected_) return;
    
    bool success = false;
    std::string error;
    
    if (useNetwork_) {
        if (buttonNetwork_) {
            success = buttonNetwork_->write(false);
            error = buttonNetwork_->getLastError();
        }
    } else {
        if (buttonControl_) {
            success = buttonControl_->release();
            error = buttonControl_->getLastError();
        }
    }
    
    if (success) {
        updateStatusBar("Button released");
        logMessage("Button released (GPIO 3 LOW)");
        emit buttonReleased();
    } else {
        QMessageBox::warning(this, "Button Control Failed",
                           QString("Failed to release button:\n%1")
                           .arg(QString::fromStdString(error)));
    }
}

void MainWindow::onButtonClick()
{
    if (!isConnected_) return;
    
    bool success = false;
    std::string error;
    
    if (useNetwork_) {
        if (buttonNetwork_) {
            // Simulate click: press, wait, release
            success = buttonNetwork_->write(true);
            if (success) {
                QThread::msleep(100);
                success = buttonNetwork_->write(false);
            }
            error = buttonNetwork_->getLastError();
        }
    } else {
        if (buttonControl_) {
            success = buttonControl_->click(100);
            error = buttonControl_->getLastError();
        }
    }
    
    if (success) {
        updateStatusBar("Button clicked");
        logMessage("Button clicked (100ms press)");
        emit buttonPressed();
        emit buttonReleased();
    } else {
        QMessageBox::warning(this, "Button Control Failed",
                           QString("Failed to click button:\n%1")
                           .arg(QString::fromStdString(error)));
    }
}

void MainWindow::onExport()
{
    // Placeholder for export functionality
    QMessageBox::information(this, "Export",
                           "Export functionality will be implemented in a future phase.");
}

void MainWindow::onSettings()
{
    // Placeholder for settings dialog
    QMessageBox::information(this, "Settings",
                           "Settings dialog will be implemented in a future phase.");
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About Rebear",
                      "<h2>Rebear - Teddy Bear Reverse Engineering</h2>"
                      "<p>Version 1.0.0</p>"
                      "<p>A tool for monitoring and patching Flash memory access "
                      "in storytelling teddy bears.</p>"
                      "<p><b>Features:</b></p>"
                      "<ul>"
                      "<li>Real-time transaction monitoring</li>"
                      "<li>Virtual patch management</li>"
                      "<li>Button control via GPIO</li>"
                      "<li>Address visualization</li>"
                      "</ul>"
                      "<p>Built with Qt and C++17</p>");
}

void MainWindow::onConnectionStatusChanged()
{
    // Placeholder for future use
}

void MainWindow::saveConnectionSettings()
{
    QSettings settings("Rebear", "RebearGUI");
    settings.setValue("connection/mode", useNetwork_ ? "network" : "local");
    settings.setValue("connection/host", remoteHost_);
    settings.setValue("connection/port", remotePort_);
    settings.setValue("connection/device", currentDevice_);
    settings.setValue("connection/speed", currentSpeed_);
}

void MainWindow::loadConnectionSettings()
{
    QSettings settings("Rebear", "RebearGUI");
    QString mode = settings.value("connection/mode", "local").toString();
    useNetwork_ = (mode == "network");
    remoteHost_ = settings.value("connection/host", "raspberrypi.local").toString();
    remotePort_ = settings.value("connection/port", 9876).toUInt();
    currentDevice_ = settings.value("connection/device", "/dev/spidev0.0").toString();
    currentSpeed_ = settings.value("connection/speed", 100000).toUInt();
}
