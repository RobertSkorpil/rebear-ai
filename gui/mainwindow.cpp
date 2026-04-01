#include "mainwindow.h"
#include "widgets/transaction_viewer.h"
#include "widgets/patch_editor.h"
#include "widgets/hex_viewer.h"
#include "widgets/connection_dialog.h"
#if defined(__linux__) && !defined(__APPLE__)
#include "rebear/spi_protocol.h"
#include "rebear/gpio_control.h"
#endif
#include "rebear/spi_protocol_network.h"
#include "rebear/patch.h"
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
#include <algorithm>
#include <set>
#include <QSettings>
#include <QThread>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

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
    
    // Create polling timers
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
#if defined(__linux__) && !defined(__APPLE__)
            if (spi_) spi_->close();
#endif
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
    
    refreshPatchesAction_ = new QAction(tr("&Refresh Patches"), this);
    refreshPatchesAction_->setShortcut(QKeySequence(tr("Ctrl+R")));
    refreshPatchesAction_->setStatusTip(tr("Refresh patches from FPGA"));
    connect(refreshPatchesAction_, &QAction::triggered, this, &MainWindow::onRefreshPatches);
    
    dumpPatchBufferAction_ = new QAction(tr("&Dump Patch Buffer"), this);
    dumpPatchBufferAction_->setStatusTip(tr("Dump patch buffer content from FPGA"));
    connect(dumpPatchBufferAction_, &QAction::triggered, this, &MainWindow::onDumpPatchBuffer);
    
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
    toolsMenu_->addAction(refreshPatchesAction_);
    toolsMenu_->addAction(dumpPatchBufferAction_);
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
    mainToolBar_->addAction(refreshPatchesAction_);
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
    transactionViewer_->setPatches(&patches_);
    
    // Create patch editor
    patchEditor_ = new rebear::gui::PatchEditor(this);
    patchEditor_->setPatches(&patches_);
    
    // Add to left layout (50/50 split vertically)
    leftLayout->addWidget(transactionViewer_);
    leftLayout->addWidget(patchEditor_);
    
    // Create hex viewer
    hexViewer_ = new rebear::gui::HexViewer(this);
    hexViewer_->setPatches(&patches_);
    
    // Add to horizontal layout (40% left, 60% hex viewer)
    topLayout->addWidget(leftWidget, 2);
    topLayout->addWidget(hexViewer_, 3);
    
    // Create log widget
    logWidget_ = new QTextEdit(this);
    logWidget_->setReadOnly(true);
    logWidget_->setMaximumHeight(200);
    
    // Create address encoder
    QWidget* encoderWidget = createAddressEncoder();
    
    // Create bottom area with log and encoder side by side
    QWidget* bottomWidget = new QWidget(this);
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->addWidget(logWidget_, 3);
    bottomLayout->addWidget(encoderWidget, 1);
    
    // Add widgets to main splitter
    mainSplitter_->addWidget(topWidget);
    mainSplitter_->addWidget(bottomWidget);
    
    // Set splitter sizes (80% top, 20% bottom)
    mainSplitter_->setStretchFactor(0, 4);
    mainSplitter_->setStretchFactor(1, 1);
    
    // Connect signals
    connect(this, &MainWindow::transactionReceived,
            transactionViewer_, &rebear::gui::TransactionViewer::addTransaction);
    
    // Connect transaction viewer to hex viewer
    connect(transactionViewer_, &rebear::gui::TransactionViewer::transactionClicked,
            hexViewer_, &rebear::gui::HexViewer::onTransactionClicked);
    
    // Connect hex viewer patch creation to patch list
    connect(hexViewer_, &rebear::gui::HexViewer::patchCreated,
            this, [this](const rebear::Patch& patch) {
                if (patch.isValid()) {
                    patches_.push_back(patch);
                    patchEditor_->refresh();
                    hexViewer_->refresh();
                    transactionViewer_->setPatches(&patches_);
                    logMessage(QString("Created patch ID %1 at address 0x%2")
                              .arg(patch.id)
                              .arg(patch.address, 6, 16, QChar('0')).toUpper());
                } else {
                    QMessageBox::warning(this, "Patch Error", "Invalid patch data");
                }
            });
    
    connect(hexViewer_, &rebear::gui::HexViewer::autoApplyPatches,
            this, [this]() {
                if (!isConnected_) return;
                bool success = false;
                if (useNetwork_) {
                    success = spiNetwork_->uploadPatchBuffer(patches_);
                } else {
#if defined(__linux__) && !defined(__APPLE__)
                    success = spi_->uploadPatchBuffer(patches_);
#endif
                }
                if (success) {
                    updateStatusBar("Patches auto-applied");
                    logMessage("Patches automatically applied to FPGA");
                    hexViewer_->refresh();
                    // Refresh from FPGA to confirm
                    refreshPatchesFromFPGA();
                } else {
                    QString errorMsg = useNetwork_ ? 
                        QString::fromStdString(spiNetwork_->getLastError()) :
#if defined(__linux__) && !defined(__APPLE__)
                        QString::fromStdString(spi_->getLastError());
#else
                        QString("Not supported");
#endif
                    logMessage(QString("Auto-apply failed: %1").arg(errorMsg));
                }
            });
    
    connect(hexViewer_, &rebear::gui::HexViewer::clearAutoPatches,
            this, [this](const std::set<uint8_t>& patchIds) {
                // Remove patches with matching IDs
                patches_.erase(
                    std::remove_if(patches_.begin(), patches_.end(),
                        [&patchIds](const rebear::Patch& p) {
                            return patchIds.find(p.id) != patchIds.end();
                        }),
                    patches_.end()
                );
                patchEditor_->refresh();
                hexViewer_->refresh();
                transactionViewer_->setPatches(&patches_);
            });
    
    connect(patchEditor_, &rebear::gui::PatchEditor::applyAllRequested,
            this, [this]() {
                if (!isConnected_) return;
                bool success = false;
                if (useNetwork_) {
                    success = spiNetwork_->uploadPatchBuffer(patches_);
                } else {
#if defined(__linux__) && !defined(__APPLE__)
                    success = spi_->uploadPatchBuffer(patches_);
#endif
                }
                if (success) {
                    updateStatusBar("All patches applied");
                    logMessage("All patches applied to FPGA");
                    hexViewer_->refresh();
                    // Refresh from FPGA to confirm
                    refreshPatchesFromFPGA();
                } else {
                    QString errorMsg = useNetwork_ ? 
                        QString::fromStdString(spiNetwork_->getLastError()) :
#if defined(__linux__) && !defined(__APPLE__)
                        QString::fromStdString(spi_->getLastError());
#else
                        QString("Not supported");
#endif
                    QMessageBox::warning(this, "Apply Failed", errorMsg);
                }
            });
    
    connect(patchEditor_, &rebear::gui::PatchEditor::clearAllRequested,
            this, &MainWindow::onClearPatches);
    
    connect(patchEditor_, &rebear::gui::PatchEditor::patchesChanged,
            hexViewer_, &rebear::gui::HexViewer::refresh);
    
    connect(patchEditor_, &rebear::gui::PatchEditor::patchesChanged,
            this, [this]() {
                transactionViewer_->setPatches(&patches_);
            });
    
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
    refreshPatchesAction_->setEnabled(connected);
    dumpPatchBufferAction_->setEnabled(connected);
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
#if defined(__linux__) && !defined(__APPLE__)
        // Local mode - Linux only
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
#else
        // Local mode not supported on non-Linux platforms
        QMessageBox::critical(this, "Connection Error",
            "Local hardware mode is only supported on Linux.\nPlease use network mode to connect to a remote server.");
        logMessage("Error: Local mode not supported on this platform. Use network mode.");
        return;
#endif
    }
    
    // Save settings if requested
    if (dialog.shouldRemember()) {
        saveConnectionSettings();
    }
    
    // Start polling timers
    pollTimer_->start(100); // 100ms for transactions
    
    // Immediately fetch current patches from FPGA
    refreshPatchesFromFPGA();
    
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
#if defined(__linux__) && !defined(__APPLE__)
        if (spi_) {
            spi_->close();
            spi_.reset();
        }
        // ButtonControl and BufferReadyMonitor don't have close() - destructor handles cleanup
        buttonControl_.reset();
        bufferMonitor_.reset();
        logMessage("Disconnected from FPGA");
#endif
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
#if defined(__linux__) && !defined(__APPLE__)
        success = spi_->clearTransactions();
        error = spi_->getLastError();
#endif
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
            ready = bufferMonitorNetwork_->readInput();
        }
    } else {
#if defined(__linux__) && !defined(__APPLE__)
        if (bufferMonitor_) {
            ready = bufferMonitor_->isReady();
        }
#endif
    }
    
    if (!ready) {
        return;
    }
    
    // Read ALL available transactions in the buffer
    // Keep reading while buffer has data
    for(int i = 0; i < 10; i++) {
        std::optional<rebear::Transaction> trans;
        
        if (useNetwork_) {
            trans = spiNetwork_->readTransaction();
        } else {
#if defined(__linux__) && !defined(__APPLE__)
            trans = spi_->readTransaction();
#endif
        }
        
        if (trans && trans->address != 0xFFFFFF) {
            transactionCount_++;
            transactionCountLabel_->setText(QString("Transactions: %1").arg(transactionCount_));
            
            // Log transaction
            logMessage(QString("Transaction: Addr=0x%1 Count=%2 Time=%3ms")
                      .arg(trans->address, 6, 16, QChar('0'))
                      .arg(trans->count)
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
    QMessageBox::information(this, "Not Implemented", 
        "Patch file I/O has been removed. Create patches interactively in the hex viewer.");
}

void MainWindow::onSavePatches()
{
    QMessageBox::information(this, "Not Implemented", 
        "Patch file I/O has been removed. Patches exist only in memory during the session.");
}

void MainWindow::onClearPatches()
{
    if (!isConnected_) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                             "Clear Patches",
                                                             "Clear all patches from FPGA?",
                                                             QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        patches_.clear();
        patchEditor_->refresh();
        
        bool cleared = false;
        if (useNetwork_) {
            cleared = spiNetwork_->clearPatches();
        } else {
#if defined(__linux__) && !defined(__APPLE__)
            cleared = spi_->clearPatches();
#endif
        }
        
        if (cleared) {
            updateStatusBar("All patches cleared");
            logMessage("All patches cleared from FPGA and local memory");
            // Refresh from FPGA to confirm
            refreshPatchesFromFPGA();
        } else {
            QString errorMsg = useNetwork_ ? 
                QString::fromStdString(spiNetwork_->getLastError()) :
#if defined(__linux__) && !defined(__APPLE__)
                QString::fromStdString(spi_->getLastError());
#else
                QString("Not supported");
#endif
            QMessageBox::warning(this, "Clear Failed",
                               QString("Failed to clear patches:\n%1").arg(errorMsg));
        }
    }
}

void MainWindow::onDumpPatchBuffer()
{
    if (!isConnected_) return;
    
    std::vector<uint8_t> buffer;
    bool success = false;
    
    if (useNetwork_) {
        success = spiNetwork_->dumpPatchBuffer(buffer);
    } else {
#if defined(__linux__) && !defined(__APPLE__)
        success = spi_->dumpPatchBuffer(buffer);
#endif
    }
    
    if (success) {
        // Display the buffer in the hex viewer
        if (buffer.empty()) {
            QMessageBox::information(this, "Patch Buffer", "Patch buffer is empty.");
            logMessage("Patch buffer dump: buffer is empty");
        } else {
            // Show buffer in hex viewer
            hexViewer_->setFlashData(buffer);
            
            // Log summary
            QString message = QString("Patch buffer dumped: %1 bytes").arg(buffer.size());
            updateStatusBar(message);
            logMessage(message);
            
            // Show info dialog with summary
            QMessageBox::information(this, "Patch Buffer Dump",
                                   QString("Successfully dumped patch buffer:\n"
                                          "Size: %1 bytes\n\n"
                                          "Buffer content is displayed in the Hex Viewer tab.")
                                   .arg(buffer.size()));
        }
    } else {
        QString errorMsg;
        if (useNetwork_) {
            errorMsg = QString::fromStdString(spiNetwork_->getLastError());
        } else {
#if defined(__linux__) && !defined(__APPLE__)
            errorMsg = QString::fromStdString(spi_->getLastError());
#endif
        }
        
        QMessageBox::warning(this, "Dump Failed",
                           QString("Failed to dump patch buffer:\n%1").arg(errorMsg));
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
#if defined(__linux__) && !defined(__APPLE__)
        if (buttonControl_) {
            success = buttonControl_->press();
            error = buttonControl_->getLastError();
        }
#endif
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
#if defined(__linux__) && !defined(__APPLE__)
        if (buttonControl_) {
            success = buttonControl_->release();
            error = buttonControl_->getLastError();
        }
#endif
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
#if defined(__linux__) && !defined(__APPLE__)
        if (buttonControl_) {
            success = buttonControl_->click(100);
            error = buttonControl_->getLastError();
        }
#endif
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

void MainWindow::refreshPatchesFromFPGA()
{
    if (!isConnected_) {
        return;
    }
    
    // Dump patch buffer from FPGA
    std::vector<uint8_t> buffer;
    bool success = false;
    
    if (useNetwork_) {
        success = spiNetwork_ && spiNetwork_->dumpPatchBuffer(buffer);
    } else {
#if defined(__linux__) && !defined(__APPLE__)
        success = spi_ && spi_->dumpPatchBuffer(buffer);
#endif
    }
    
    if (!success) {
        logMessage("Warning: Failed to refresh patches from FPGA");
        return;
    }
    
    if (buffer.empty()) {
        // No patches in FPGA - clear local list
        if (!patches_.empty()) {
            patches_.clear();
            patchEditor_->refresh();
            hexViewer_->refresh();
            logMessage("Patches cleared (FPGA has no patches)");
        }
        return;
    }
    
    // Parse the buffer to extract patches
    std::vector<rebear::Patch> newPatches;
    size_t offset = 0;
    uint8_t patchId = 0;
    
    // Parse headers
    while (offset < buffer.size()) {
        // Read STORED byte
        uint8_t stored = buffer[offset++];
        
        // Check for terminator
        if (stored == 0x00 && offset >= buffer.size()) {
            break;
        }
        
        if (offset + 7 > buffer.size()) {
            break;
        }
        
        // Parse patch header (8 bytes: STORED + ADDRESS(3) + LENGTH(2) + OFFSET(2))
        uint32_t address = (static_cast<uint32_t>(buffer[offset]) << 16) |
                          (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
                          static_cast<uint32_t>(buffer[offset + 2]);
        offset += 3;
        
        uint16_t length = (static_cast<uint16_t>(buffer[offset]) << 8) |
                         static_cast<uint16_t>(buffer[offset + 1]);
        offset += 2;
        
        uint16_t dataOffset = (static_cast<uint16_t>(buffer[offset]) << 8) |
                             static_cast<uint16_t>(buffer[offset + 1]);
        offset += 2;
        
        // Check for terminator
        if (stored == 0x00) {
            break;
        }
        
        // Validate data offset and length
        if (dataOffset + length > buffer.size()) {
            logMessage(QString("Warning: Invalid patch data at offset %1").arg(offset));
            break;
        }
        
        // Extract patch
        rebear::Patch patch;
        patch.id = patchId++;
        patch.address = address;
        patch.enabled = (stored == 0x80);
        patch.data.assign(buffer.begin() + dataOffset, 
                         buffer.begin() + dataOffset + length);
        
        newPatches.push_back(patch);
    }
    
    // Update local patch list if changed
    if (patches_ != newPatches) {
        patches_ = newPatches;
        patchEditor_->refresh();
        hexViewer_->refresh();
        logMessage(QString("Synced %1 patch(es) from FPGA").arg(patches_.size()));
    }
}

void MainWindow::onRefreshPatches()
{
    if (!isConnected_) return;
    
    logMessage("Manually refreshing patches from FPGA...");
    refreshPatchesFromFPGA();
    updateStatusBar("Patches refreshed from FPGA");
}

QWidget* MainWindow::createAddressEncoder()
{
    addressEncoderGroup_ = new QGroupBox("Address Encoder", this);
    QVBoxLayout* encoderLayout = new QVBoxLayout(addressEncoderGroup_);
    
    // Create input field with hex validator
    QLabel* inputLabel = new QLabel("24-bit Address (Hex):", addressEncoderGroup_);
    addressInput_ = new QLineEdit(addressEncoderGroup_);
    addressInput_->setPlaceholderText("000000");
    
    // Connect to textChanged to filter input (allows pasting with spaces)
    connect(addressInput_, &QLineEdit::textChanged, this, [this](const QString& text) {
        // Remove all non-hex characters (including spaces)
        QString filtered = text;
        filtered.remove(QRegularExpression("[^0-9A-Fa-f]"));
        
        // Limit to 6 characters
        if (filtered.length() > 6) {
            filtered = filtered.left(6);
        }
        
        // Update only if different (to avoid infinite loop)
        if (filtered != text) {
            int cursorPos = addressInput_->cursorPosition();
            addressInput_->setText(filtered);
            addressInput_->setCursorPosition(qMin(cursorPos, filtered.length()));
        }
    });
    
    // Create encode and decode buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    encodeButton_ = new QPushButton("Encode", addressEncoderGroup_);
    decodeButton_ = new QPushButton("Decode", addressEncoderGroup_);
    connect(encodeButton_, &QPushButton::clicked, this, &MainWindow::onEncodeAddress);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::onDecodeAddress);
    
    buttonLayout->addWidget(encodeButton_);
    buttonLayout->addWidget(decodeButton_);
    
    // Allow Enter key to trigger encoding
    connect(addressInput_, &QLineEdit::returnPressed, this, &MainWindow::onEncodeAddress);
    
    // Layout
    encoderLayout->addWidget(inputLabel);
    encoderLayout->addWidget(addressInput_);
    encoderLayout->addLayout(buttonLayout);
    encoderLayout->addStretch();
    
    addressEncoderGroup_->setMaximumWidth(250);
    
    return addressEncoderGroup_;
}           

uint32_t coeffs[] = {
    0x00ffff,
    0xfe0002,
    0x03fffc,
    0x07fff8,
    0x0ffff0,
    0xe00020,
    0xc00040,
    0x800080,
    0x000100,
    0xfffe00,
    0x000400,
    0x000800,
    0x001000,
    0x002000,
    0x004000,
    0x008000,
    0xff0000,
    0x020000,
    0xfc0000,
    0xf80000,
    0xf00000,
    0x200000,
    0x400000,
    0x800000,
};

void MainWindow::onEncodeAddress()
{
    QString addressText = addressInput_->text().trimmed();
    
    if (addressText.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a 24-bit hexadecimal address.");
        return;
    }
    
    // Pad with leading zeros if needed
    addressText = addressText.rightJustified(6, '0');
    
    // Parse hex value
    bool ok;
    uint32_t address = addressText.toUInt(&ok, 16);
    
    if (!ok || address > 0xFFFFFF) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid 24-bit hexadecimal address (000000-FFFFFF).");
        return;
    }
    
    uint32_t encodedValue{};
    address -= 0x21d;
    for (int i = 0; i < 24; ++i)
    {
        if (address & (1u << i))
        {
            encodedValue |= (1 << i);
            address -= coeffs[i];
        }
    }

    std::swap(((uint8_t*)&encodedValue)[0], ((uint8_t*)&encodedValue)[2]);
    
    // Update the input field with encoded value
    QString encodedText = QString("%1").arg(encodedValue, 6, 16, QChar('0')).toUpper();
    addressInput_->setText(encodedText);
    
    // Log the encoding
    logMessage(QString("Encoded address: 0x%1 -> 0x%2")
              .arg(addressText.toUpper())
              .arg(encodedText));
    
    updateStatusBar(QString("Address encoded: 0x%1").arg(encodedText));
}

void MainWindow::onDecodeAddress()
{
    QString addressText = addressInput_->text().trimmed();
    
    if (addressText.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a 24-bit hexadecimal address.");
        return;
    }
    
    // Pad with leading zeros if needed
    addressText = addressText.rightJustified(6, '0');
    
    // Parse hex value
    bool ok;
    uint32_t encodedAddress = addressText.toUInt(&ok, 16);
    
    if (!ok || encodedAddress > 0xFFFFFF) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid 24-bit hexadecimal address (000000-FFFFFF).");
        return;
    }

    std::swap(((uint8_t*)&encodedAddress)[0], ((uint8_t*)&encodedAddress)[2]);
    uint32_t decodedValue{ 0x21d };
    for (int i = 0; i < 24; ++i)
        if (encodedAddress & (1 << i))
            decodedValue += coeffs[i];

    decodedValue &= 0xffffff;
    
    // Update the input field with decoded value
    QString decodedText = QString("%1").arg(decodedValue, 6, 16, QChar('0')).toUpper();
    addressInput_->setText(decodedText);
    
    // Log the decoding
    logMessage(QString("Decoded address: 0x%1 -> 0x%2")
              .arg(addressText.toUpper())
              .arg(decodedText));
    
    updateStatusBar(QString("Address decoded: 0x%1").arg(decodedText));
}
