#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace rebear {

/**
 * @brief Dialog for selecting connection mode (local or network)
 * 
 * Allows the user to choose between local hardware access or connecting
 * to a remote rebear-server instance over the network.
 */
class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        Local,
        Network
    };

    explicit ConnectionDialog(QWidget* parent = nullptr);
    ~ConnectionDialog() override = default;

    /**
     * @brief Get the selected connection mode
     * @return Mode::Local or Mode::Network
     */
    Mode getMode() const;

    /**
     * @brief Get the hostname for network mode
     * @return Hostname or IP address
     */
    QString getHostname() const;

    /**
     * @brief Get the port for network mode
     * @return Port number (default 9876)
     */
    uint16_t getPort() const;

    /**
     * @brief Check if settings should be remembered
     * @return true if "Remember" checkbox is checked
     */
    bool shouldRemember() const;

    /**
     * @brief Set the initial mode
     * @param mode Mode to select
     */
    void setMode(Mode mode);

    /**
     * @brief Set the initial hostname
     * @param hostname Hostname to display
     */
    void setHostname(const QString& hostname);

    /**
     * @brief Set the initial port
     * @param port Port number to display
     */
    void setPort(uint16_t port);

    /**
     * @brief Set the remember checkbox state
     * @param remember true to check the box
     */
    void setRemember(bool remember);

private slots:
    void onModeChanged();
    void onTestConnection();

private:
    void setupUI();
    void updateFieldsEnabled();

    QRadioButton* localRadio_;
    QRadioButton* networkRadio_;
    QLineEdit* hostnameEdit_;
    QSpinBox* portSpin_;
    QCheckBox* rememberCheck_;
    QPushButton* testButton_;
    QLabel* statusLabel_;
};

} // namespace rebear
