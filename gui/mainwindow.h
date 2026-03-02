#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <memory>

// Forward declarations
class QAction;
class QMenu;
class QToolBar;
class QStatusBar;
class QSplitter;
class QTextEdit;

namespace rebear {
    class SPIProtocol;
    class SPIProtocolNetwork;
    class PatchManager;
    class ButtonControl;
    class BufferReadyMonitor;
    class GPIOControlNetwork;
    class ConnectionDialog;
    struct Transaction;
    
    namespace gui {
        class TransactionViewer;
        class PatchEditor;
        class HexViewer;
    }
}

/**
 * @brief Main window for the Rebear GUI application
 * 
 * Provides the main user interface for monitoring Flash memory transactions,
 * managing patches, and controlling the teddy bear button.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    /**
     * @brief Emitted when successfully connected to FPGA
     */
    void connected();
    
    /**
     * @brief Emitted when disconnected from FPGA
     */
    void disconnected();
    
    /**
     * @brief Emitted when a new transaction is received
     * @param trans The transaction data
     */
    void transactionReceived(const rebear::Transaction& trans);
    
    /**
     * @brief Emitted when a patch is applied
     * @param id The patch ID (0-15)
     */
    void patchApplied(uint8_t id);
    
    /**
     * @brief Emitted when button is pressed
     */
    void buttonPressed();
    
    /**
     * @brief Emitted when button is released
     */
    void buttonReleased();
    
    /**
     * @brief Emitted when buffer ready state changes
     * @param ready True if buffer has data available
     */
    void bufferReadyChanged(bool ready);

private slots:
    // Connection management
    void onConnect();
    void onDisconnect();
    void onConnectionStatusChanged();
    
    // Transaction management
    void onClearTransactions();
    void onPollTransactions();
    
    // Patch management
    void onLoadPatches();
    void onSavePatches();
    void onClearPatches();
    
    // Button control
    void onButtonPress();
    void onButtonRelease();
    void onButtonClick();
    
    // Export
    void onExport();
    
    // Help
    void onAbout();
    
    // Settings
    void onSettings();

private:
    // UI setup methods
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createCentralWidget();
    
    // Helper methods
    void updateConnectionState(bool connected);
    void updateStatusBar(const QString& message);
    void logMessage(const QString& message);
    void saveConnectionSettings();
    void loadConnectionSettings();
    
    // Actions
    QAction* connectAction_;
    QAction* disconnectAction_;
    QAction* clearTransactionsAction_;
    QAction* loadPatchesAction_;
    QAction* savePatchesAction_;
    QAction* clearPatchesAction_;
    QAction* exportAction_;
    QAction* exitAction_;
    QAction* settingsAction_;
    QAction* aboutAction_;
    QAction* buttonPressAction_;
    QAction* buttonReleaseAction_;
    QAction* buttonClickAction_;
    
    // Menus
    QMenu* fileMenu_;
    QMenu* editMenu_;
    QMenu* viewMenu_;
    QMenu* toolsMenu_;
    QMenu* helpMenu_;
    
    // Toolbar
    QToolBar* mainToolBar_;
    
    // Status bar widgets
    QLabel* connectionStatusLabel_;
    QLabel* transactionCountLabel_;
    QLabel* bufferStatusLabel_;
    
    // Central widget components
    QSplitter* mainSplitter_;
    QTextEdit* logWidget_;
    rebear::gui::TransactionViewer* transactionViewer_;
    rebear::gui::PatchEditor* patchEditor_;
    rebear::gui::HexViewer* hexViewer_;
    
    // Core library objects (local mode)
    std::unique_ptr<rebear::SPIProtocol> spi_;
    std::unique_ptr<rebear::PatchManager> patchManager_;
    std::unique_ptr<rebear::ButtonControl> buttonControl_;
    std::unique_ptr<rebear::BufferReadyMonitor> bufferMonitor_;
    
    // Network objects (network mode)
    std::unique_ptr<rebear::SPIProtocolNetwork> spiNetwork_;
    std::unique_ptr<rebear::GPIOControlNetwork> buttonNetwork_;
    std::unique_ptr<rebear::GPIOControlNetwork> bufferMonitorNetwork_;
    
    // Polling timer for transactions
    QTimer* pollTimer_;
    
    // State
    bool isConnected_;
    int transactionCount_;
    QString currentDevice_;
    uint32_t currentSpeed_;
    
    // Connection mode
    bool useNetwork_;
    QString remoteHost_;
    uint16_t remotePort_;
};
