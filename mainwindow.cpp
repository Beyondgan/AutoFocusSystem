#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QInputDialog>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("自动对焦控制系统"));

    m_laserSensor = new SerialManager(SerialManager::LaserSensor, this);
    m_axisController = new AxisController(this);
    m_lensController = new LiquidLensController(this);
    m_workflow = new WorkflowEngine(this);

    m_workflow->setLaserSensor(m_laserSensor);
    m_workflow->setAxisController(m_axisController);
    m_workflow->setLiquidLens(m_lensController);

    initUi();
    initConnections();
    updatePortComboBoxes();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUi()
{
    ui->laserStatusLabel->setText(tr("未连接"));
    ui->laserStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    ui->lensStatusLabel->setText(tr("未连接"));
    ui->lensStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    ui->axisStatusLabel->setText(tr("未连接"));
    ui->axisStatusLabel->setStyleSheet("color: red; font-weight: bold;");

    ui->axisSpeedSlider->setRange(1, 500);
    ui->axisSpeedSlider->setValue(100);
    ui->axisSpeedLabel->setText("100 mm/s");

    ui->lensFocusSlider->setRange(0, 1023);
    ui->lensFocusSlider->setValue(0);
    ui->lensFocusValueLabel->setText("0");

    ui->scanProgressBar->setValue(0);
    ui->workflowStateLabel->setText(tr("停止"));

    ui->distanceDisplay->setText("-- mm");
    ui->focusDisplay->setText("--");
    ui->axisPosDisplay->setText("-- mm");

    QMap<double, int> defaultCalibration;
    defaultCalibration[50.0] = 0;
    defaultCalibration[300.0] = 300;
    defaultCalibration[600.0] = 600;
    defaultCalibration[900.0] = 900;
    defaultCalibration[1000.0] = 1023;
    m_lensController->loadCalibration(defaultCalibration);
}

void MainWindow::initConnections()
{
    connect(ui->refreshPortsBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);

    connect(ui->connectLaserBtn, &QPushButton::clicked, this, &MainWindow::onConnectLaser);
    connect(ui->disconnectLaserBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectLaser);
    connect(ui->connectLensBtn, &QPushButton::clicked, this, &MainWindow::onConnectLens);
    connect(ui->disconnectLensBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectLens);
    connect(ui->connectAxisBtn, &QPushButton::clicked, this, &MainWindow::onConnectAxis);
    connect(ui->disconnectAxisBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectAxis);

    connect(ui->axisMoveBtn, &QPushButton::clicked, this, &MainWindow::onAxisMoveTo);
    connect(ui->axisHomeBtn, &QPushButton::clicked, this, &MainWindow::onAxisHome);
    connect(ui->axisStopBtn, &QPushButton::clicked, this, &MainWindow::onAxisStop);
    connect(ui->axisSpeedSlider, &QSlider::valueChanged, this, &MainWindow::onAxisSpeedChanged);

    connect(ui->lensFocusSlider, &QSlider::valueChanged, this, &MainWindow::onLensFocusChanged);
    connect(ui->lensCalibrateBtn, &QPushButton::clicked, this, &MainWindow::onLensCalibrate);

    connect(ui->singleMeasureBtn, &QPushButton::clicked, this, &MainWindow::onSingleMeasure);
    connect(ui->startScanBtn, &QPushButton::clicked, this, &MainWindow::onStartScan);
    connect(ui->pauseScanBtn, &QPushButton::clicked, this, &MainWindow::onPauseScan);
    connect(ui->resumeScanBtn, &QPushButton::clicked, this, &MainWindow::onResumeScan);
    connect(ui->stopScanBtn, &QPushButton::clicked, this, &MainWindow::onStopScan);

    connect(m_workflow, &WorkflowEngine::stateChanged, this, &MainWindow::onWorkflowStateChanged);
    connect(m_workflow, &WorkflowEngine::distanceMeasured, this, &MainWindow::onDistanceMeasured);
    connect(m_workflow, &WorkflowEngine::scanPointCompleted, this, &MainWindow::onScanPointCompleted);
    connect(m_workflow, &WorkflowEngine::scanFinished, this, &MainWindow::onScanFinished);
    connect(m_workflow, &WorkflowEngine::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_workflow, &WorkflowEngine::logMessage, this, &MainWindow::onLogMessage);
    connect(m_workflow, &WorkflowEngine::errorOccurred, this, [this](const QString &err) {
        appendLog(tr("[错误] %1").arg(err));
    });

    connect(m_laserSensor, &SerialManager::dataReceived, this, &MainWindow::onLaserDataReceived);
    connect(m_laserSensor, &SerialManager::connectionChanged, this, [this](bool connected) {
        Q_UNUSED(connected)
        updateConnectionStatus();
    });

    connect(m_axisController, &AxisController::positionChanged, this, [this](double pos) {
        ui->axisPosDisplay->setText(QString("%1 mm").arg(pos, 0, 'f', 2));
    });
    connect(m_axisController, &AxisController::stateChanged, this, [this](AxisController::AxisState state) {
        ui->axisStateLabel->setText(
            state == AxisController::Idle ? tr("空闲") :
            state == AxisController::Moving ? tr("运动中") :
            state == AxisController::Homing ? tr("回零中") :
            state == AxisController::Error ? tr("错误") : tr("未知"));
    });

    connect(m_lensController, &LiquidLensController::focusChanged, this, [this](int val) {
        ui->lensFocusValueLabel->setText(QString::number(val));
        ui->focusDisplay->setText(QString::number(val));
    });
}

void MainWindow::onRefreshPorts()
{
    updatePortComboBoxes();
}

void MainWindow::onConnectLaser()
{
    QString port = ui->laserPortCombo->currentText();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择激光传感器串口"));
        return;
    }
    qint32 baud = ui->laserBaudCombo->currentText().toInt();
    if (m_laserSensor->open(port, baud)) {
        appendLog(tr("激光传感器已连接: %1").arg(port));
    }
    updateConnectionStatus();
}

void MainWindow::onDisconnectLaser()
{
    m_laserSensor->close();
    appendLog(tr("激光传感器已断开"));
    updateConnectionStatus();
}

void MainWindow::onConnectLens()
{
    QString port = ui->lensPortCombo->currentText();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择液态镜头串口"));
        return;
    }
    qint32 baud = ui->lensBaudCombo->currentText().toInt();
    if (m_lensController->connectLens(port, baud)) {
        appendLog(tr("液态镜头已连接: %1").arg(port));
    }
    updateConnectionStatus();
}

void MainWindow::onDisconnectLens()
{
    m_lensController->disconnectLens();
    appendLog(tr("液态镜头已断开"));
    updateConnectionStatus();
}

void MainWindow::onConnectAxis()
{
    QString port = ui->axisPortCombo->currentText();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择运动轴串口"));
        return;
    }
    qint32 baud = ui->axisBaudCombo->currentText().toInt();
    if (m_axisController->connectAxis(port, baud)) {
        appendLog(tr("运动轴已连接: %1").arg(port));
    }
    updateConnectionStatus();
}

void MainWindow::onDisconnectAxis()
{
    m_axisController->disconnectAxis();
    appendLog(tr("运动轴已断开"));
    updateConnectionStatus();
}

void MainWindow::onAxisMoveTo()
{
    double pos = ui->axisPosSpin->value();
    m_axisController->moveToPosition(pos);
    appendLog(tr("移动到位置: %1 mm").arg(pos, 0, 'f', 2));
}

void MainWindow::onAxisHome()
{
    m_axisController->home();
    appendLog(tr("轴回零"));
}

void MainWindow::onAxisStop()
{
    m_axisController->stop();
    appendLog(tr("轴停止"));
}

void MainWindow::onAxisSpeedChanged(int value)
{
    ui->axisSpeedLabel->setText(QString("%1 mm/s").arg(value));
    m_axisController->setSpeed(value);
}

void MainWindow::onLensFocusChanged(int value)
{
    m_lensController->setFocusDirect(value);
}

void MainWindow::onLensCalibrate()
{
    bool ok1 = false, ok2 = false;
    double dist = QInputDialog::getDouble(this, tr("标定"), tr("距离 (mm):"), 100, 0, 9999, 2, &ok1);
    if (!ok1) return;
    int focus = QInputDialog::getInt(this, tr("标定"), tr("焦距值:"), 512, 0, 1023, 1, &ok2);
    if (!ok2) return;

    m_lensController->addCalibrationPoint(dist, focus);
    appendLog(tr("添加标定点: 距离=%1mm, 焦距=%2").arg(dist, 0, 'f', 1).arg(focus));
}

void MainWindow::onSingleMeasure()
{
    m_workflow->startSinglePoint();
}

void MainWindow::onStartScan()
{
    double start = ui->scanStartSpin->value();
    double end = ui->scanEndSpin->value();
    double step = ui->scanStepSpin->value();

    if (step <= 0) {
        QMessageBox::warning(this, tr("警告"), tr("步进值必须大于0"));
        return;
    }

    m_workflow->startContinuousScan(start, end, step);
}

void MainWindow::onPauseScan()
{
    m_workflow->pause();
}

void MainWindow::onResumeScan()
{
    m_workflow->resume();
}

void MainWindow::onStopScan()
{
    m_workflow->stop();
}

void MainWindow::onWorkflowStateChanged(WorkflowEngine::WorkflowState state)
{
    QString stateStr;
    switch (state) {
    case WorkflowEngine::Stopped: stateStr = tr("停止"); break;
    case WorkflowEngine::Initializing: stateStr = tr("初始化"); break;
    case WorkflowEngine::WaitingForAxis: stateStr = tr("等待轴到位"); break;
    case WorkflowEngine::Measuring: stateStr = tr("测量中"); break;
    case WorkflowEngine::AdjustingFocus: stateStr = tr("调焦中"); break;
    case WorkflowEngine::Paused: stateStr = tr("已暂停"); break;
    case WorkflowEngine::Error: stateStr = tr("错误"); break;
    }
    ui->workflowStateLabel->setText(stateStr);
}

void MainWindow::onDistanceMeasured(double distance, int focusValue)
{
    ui->distanceDisplay->setText(QString("%1 mm").arg(distance, 0, 'f', 2));
    ui->focusDisplay->setText(QString::number(focusValue));
}

void MainWindow::onScanPointCompleted(const WorkflowEngine::ScanPoint &point)
{
    int row = ui->resultTable->rowCount();
    ui->resultTable->insertRow(row);
    ui->resultTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    ui->resultTable->setItem(row, 1, new QTableWidgetItem(QString("%1").arg(point.position, 0, 'f', 2)));
    ui->resultTable->setItem(row, 2, new QTableWidgetItem(QString("%1").arg(point.measuredDistance, 0, 'f', 2)));
    ui->resultTable->setItem(row, 3, new QTableWidgetItem(QString::number(point.focusValue)));
    ui->resultTable->scrollToBottom();
}

void MainWindow::onScanFinished()
{
    appendLog(tr("扫描完成"));
}

void MainWindow::onProgressChanged(int current, int total)
{
    ui->scanProgressBar->setMaximum(total);
    ui->scanProgressBar->setValue(current);
}

void MainWindow::onLogMessage(const QString &msg)
{
    appendLog(msg);
}

void MainWindow::onLaserDataReceived(const QByteArray &data)
{
    QString text = QString::fromUtf8(data).trimmed();
    if (!text.isEmpty()) {
        bool ok = false;
        double val = text.toDouble(&ok);
        if (ok && val >= 0) {
            ui->distanceDisplay->setText(QString("%1 mm").arg(val, 0, 'f', 2));
        }
    }
}

void MainWindow::updatePortComboBoxes()
{
    QStringList ports = SerialManager::availablePorts();

    ui->laserPortCombo->clear();
    ui->lensPortCombo->clear();
    ui->axisPortCombo->clear();

    ui->laserPortCombo->addItems(ports);
    ui->lensPortCombo->addItems(ports);
    ui->axisPortCombo->addItems(ports);
}

void MainWindow::updateConnectionStatus()
{
    if (m_laserSensor->isOpen()) {
        ui->laserStatusLabel->setText(tr("已连接"));
        ui->laserStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        ui->laserStatusLabel->setText(tr("未连接"));
        ui->laserStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    if (m_lensController->isConnected()) {
        ui->lensStatusLabel->setText(tr("已连接"));
        ui->lensStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        ui->lensStatusLabel->setText(tr("未连接"));
        ui->lensStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    if (m_axisController->isConnected()) {
        ui->axisStatusLabel->setText(tr("已连接"));
        ui->axisStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        ui->axisStatusLabel->setText(tr("未连接"));
        ui->axisStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainWindow::appendLog(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, msg));
}
