#include "connection_dialog.h"
#include "rebear/network_client.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QApplication>

namespace rebear {

ConnectionDialog::ConnectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Connection Settings");
    setupUI();
    updateFieldsEnabled();
}

void ConnectionDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Mode selection group
    auto* modeGroup = new QGroupBox("Connection Mode", this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    localRadio_ = new QRadioButton("Local Hardware", this);
    networkRadio_ = new QRadioButton("Network (Remote Server)", this);
    localRadio_->setChecked(true);

    modeLayout->addWidget(localRadio_);
    modeLayout->addWidget(networkRadio_);

    mainLayout->addWidget(modeGroup);

    // Network settings group
    auto* networkGroup = new QGroupBox("Network Settings", this);
    auto* networkLayout = new QFormLayout(networkGroup);

    hostnameEdit_ = new QLineEdit(this);
    hostnameEdit_->setPlaceholderText("raspberrypi.local");
    networkLayout->addRow("Hostname:", hostnameEdit_);

    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(9876);
    networkLayout->addRow("Port:", portSpin_);

    // Test connection button
    auto* testLayout = new QHBoxLayout();
    testButton_ = new QPushButton("Test Connection", this);
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    testLayout->addWidget(testButton_);
    testLayout->addWidget(statusLabel_, 1);
    networkLayout->addRow(testLayout);

    mainLayout->addWidget(networkGroup);

    // Remember checkbox
    rememberCheck_ = new QCheckBox("Remember these settings", this);
    rememberCheck_->setChecked(true);
    mainLayout->addWidget(rememberCheck_);

    // Dialog buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(localRadio_, &QRadioButton::toggled, this, &ConnectionDialog::onModeChanged);
    connect(networkRadio_, &QRadioButton::toggled, this, &ConnectionDialog::onModeChanged);
    connect(testButton_, &QPushButton::clicked, this, &ConnectionDialog::onTestConnection);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setMinimumWidth(400);
}

void ConnectionDialog::onModeChanged() {
    updateFieldsEnabled();
    statusLabel_->clear();
}

void ConnectionDialog::updateFieldsEnabled() {
    bool networkMode = networkRadio_->isChecked();
    hostnameEdit_->setEnabled(networkMode);
    portSpin_->setEnabled(networkMode);
    testButton_->setEnabled(networkMode);
}

void ConnectionDialog::onTestConnection() {
    QString hostname = hostnameEdit_->text().trimmed();
    if (hostname.isEmpty()) {
        hostname = "raspberrypi.local";
    }

    uint16_t port = static_cast<uint16_t>(portSpin_->value());

    statusLabel_->setText("Testing connection...");
    statusLabel_->setStyleSheet("");
    testButton_->setEnabled(false);
    QApplication::processEvents();

    // Try to connect
    NetworkClient client(hostname.toStdString(), port);
    if (client.connect()) {
        statusLabel_->setText("✓ Connection successful!");
        statusLabel_->setStyleSheet("color: green;");
        client.disconnect();
    } else {
        statusLabel_->setText("✗ Connection failed: " +
            QString::fromStdString(client.getLastError()));
        statusLabel_->setStyleSheet("color: red;");
    }

    testButton_->setEnabled(true);
}

ConnectionDialog::Mode ConnectionDialog::getMode() const {
    return networkRadio_->isChecked() ? Mode::Network : Mode::Local;
}

QString ConnectionDialog::getHostname() const {
    QString hostname = hostnameEdit_->text().trimmed();
    return hostname.isEmpty() ? "raspberrypi.local" : hostname;
}

uint16_t ConnectionDialog::getPort() const {
    return static_cast<uint16_t>(portSpin_->value());
}

bool ConnectionDialog::shouldRemember() const {
    return rememberCheck_->isChecked();
}

void ConnectionDialog::setMode(Mode mode) {
    if (mode == Mode::Network) {
        networkRadio_->setChecked(true);
    } else {
        localRadio_->setChecked(true);
    }
}

void ConnectionDialog::setHostname(const QString& hostname) {
    hostnameEdit_->setText(hostname);
}

void ConnectionDialog::setPort(uint16_t port) {
    portSpin_->setValue(port);
}

void ConnectionDialog::setRemember(bool remember) {
    rememberCheck_->setChecked(remember);
}

} // namespace rebear
