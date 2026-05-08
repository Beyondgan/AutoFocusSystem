/**
 * @file mainwindow.cpp
 * @brief 主窗口实现文件
 *
 * 实现MainWindow类的所有功能，包括：
 *   - 界面初始化和布局
 *   - 信号槽连接
 *   - 设备连接管理
 *   - 用户交互处理
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDateTime>
#include <QInputDialog>
#include <QSerialPortInfo>

/**
 * @brief 构造函数
 *
 * 初始化主窗口，创建各个硬件控制器实例，
 * 设置UI并建立信号槽连接。
 *
 * @param parent 父窗口指针
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // 设置UI界面
    ui->setupUi(this);
    setWindowTitle(tr("自动对焦控制系统"));

    // 创建各个硬件控制器实例
    // 激光传感器 - 通过串口通信
    m_laserSensor = new SerialManager(SerialManager::LaserSensor, this);

    // FMC4030运动轴控制器 - 通过网口通信
    m_axisController = new AxisController(this);

    // 液态镜头控制器 - 通过串口通信
    m_lensController = new LiquidLensController(this);

    // 工作流引擎 - 协调各模块工作
    m_workflow = new WorkflowEngine(this);

    // 配置工作流引擎的组件
    m_workflow->setLaserSensor(m_laserSensor);
    m_workflow->setAxisController(m_axisController);
    m_workflow->setLiquidLens(m_lensController);

    // 初始化UI和信号槽连接
    initUi();
    initConnections();
    updatePortComboBoxes();
}

/**
 * @brief 析构函数
 */
MainWindow::~MainWindow()
{
    // ui会自动在父对象销毁时一起销毁
    delete ui;
}

/**
 * @brief 初始化用户界面
 *
 * 设置各个控件的初始状态和默认值
 */
void MainWindow::initUi()
{
    // ============ 连接状态显示 ============
    // 激光传感器状态 - 初始为未连接
    ui->laserStatusLabel->setText(tr("未连接"));
    ui->laserStatusLabel->setStyleSheet("color: red; font-weight: bold;");

    // 液态镜头状态 - 初始为未连接
    ui->lensStatusLabel->setText(tr("未连接"));
    ui->lensStatusLabel->setStyleSheet("color: red; font-weight: bold;");

    // FMC4030状态 - 初始为未连接
    ui->axisStatusLabel->setText(tr("未连接"));
    ui->axisStatusLabel->setStyleSheet("color: red; font-weight: bold;");

    // ============ 轴控制参数 ============
    // 轴速度滑块范围: 1 ~ 500 mm/s
    ui->axisSpeedSlider->setRange(1, 500);
    ui->axisSpeedSlider->setValue(10);  // 默认速度10mm/s
    ui->axisSpeedLabel->setText("10 mm/s");

    // ============ 液态镜头参数 ============
    // 焦距值范围: 0 ~ 1023 (10位DAC)
    ui->lensFocusSlider->setRange(0, 1023);
    ui->lensFocusSlider->setValue(0);
    ui->lensFocusValueLabel->setText("0");

    // ============ 工作流状态 ============
    ui->scanProgressBar->setValue(0);  // 进度条归零
    ui->workflowStateLabel->setText(tr("停止"));  // 工作流停止状态

    // ============ 实时数据显示区 ============
    // 测量距离显示 - 初始为"-- mm"
    ui->distanceDisplay->setText("-- mm");

    // 焦距值显示 - 初始为"--"
    ui->focusDisplay->setText("--");

    // 轴位置显示 - 初始显示"X/Y/Z: --"
    ui->axisXDisplay->setText("X: --");
    ui->axisYDisplay->setText("Y: --");
    ui->axisZDisplay->setText("Z: --");

    // ============ 默认标定曲线 ============
    // 设置默认的距离-焦距标定曲线
    // 用于在没有用户标定的情况下进行线性插值
    QMap<double, int> defaultCalibration;
    defaultCalibration[50.0] = 0;      // 50mm距离对应焦距0
    defaultCalibration[300.0] = 300;   // 300mm距离对应焦距300
    defaultCalibration[600.0] = 600;   // 600mm距离对应焦距600
    defaultCalibration[900.0] = 900;   // 900mm距离对应焦距900
    defaultCalibration[1000.0] = 1023;  // 1000mm距离对应焦距1023
    m_lensController->loadCalibration(defaultCalibration);

    // 默认偏移量
    ui->lensOffsetSpin->setValue(0.0);
}

/**
 * @brief 初始化信号槽连接
 *
 * 将UI控件信号与处理函数连接，
 * 将控制器信号与UI更新函数连接
 */
void MainWindow::initConnections()
{
    // ============ 串口刷新按钮 ============
    connect(ui->refreshPortsBtn, &QPushButton::clicked,
            this, &MainWindow::onRefreshPorts);

    // ============ 激光传感器连接按钮 ============
    connect(ui->connectLaserBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectLaser);
    connect(ui->disconnectLaserBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectLaser);

    // ============ 液态镜头连接按钮 ============
    connect(ui->connectLensBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectLens);
    connect(ui->disconnectLensBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectLens);

    // ============ FMC4030连接按钮 ============
    connect(ui->connectAxisBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectAxis);
    connect(ui->disconnectAxisBtn, &QPushButton::clicked,
            this, &MainWindow::onDisconnectAxis);

    // ============ 轴控制按钮 ============
    connect(ui->axisMoveBtn, &QPushButton::clicked,
            this, &MainWindow::onAxisMoveTo);
    connect(ui->axisHomeBtn, &QPushButton::clicked,
            this, &MainWindow::onAxisHome);
    connect(ui->axisStopBtn, &QPushButton::clicked,
            this, &MainWindow::onAxisStop);
    connect(ui->axisSpeedSlider, &QSlider::valueChanged,
            this, &MainWindow::onAxisSpeedChanged);

    // ============ 镜头控制 ============
    connect(ui->lensFocusSlider, &QSlider::valueChanged,
            this, &MainWindow::onLensFocusChanged);
    connect(ui->lensCalibrateBtn, &QPushButton::clicked,
            this, &MainWindow::onLensCalibrate);
    connect(ui->lensOffsetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onLensOffsetChanged);
    connect(ui->lensFreqSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &MainWindow::onLensFreqChanged);

    // ============ 工作流控制按钮 ============
    connect(ui->singleMeasureBtn, &QPushButton::clicked,
            this, &MainWindow::onSingleMeasure);
    connect(ui->startScanBtn, &QPushButton::clicked,
            this, &MainWindow::onStartScan);
    connect(ui->pauseScanBtn, &QPushButton::clicked,
            this, &MainWindow::onPauseScan);
    connect(ui->resumeScanBtn, &QPushButton::clicked,
            this, &MainWindow::onResumeScan);
    connect(ui->stopScanBtn, &QPushButton::clicked,
            this, &MainWindow::onStopScan);

    // ============ 工作流引擎信号连接 ============
    connect(m_workflow, &WorkflowEngine::stateChanged,
            this, &MainWindow::onWorkflowStateChanged);
    connect(m_workflow, &WorkflowEngine::distanceMeasured,
            this, &MainWindow::onDistanceMeasured);
    connect(m_workflow, &WorkflowEngine::scanPointCompleted,
            this, &MainWindow::onScanPointCompleted);
    connect(m_workflow, &WorkflowEngine::scanFinished,
            this, &MainWindow::onScanFinished);
    connect(m_workflow, &WorkflowEngine::progressChanged,
            this, &MainWindow::onProgressChanged);
    connect(m_workflow, &WorkflowEngine::logMessage,
            this, &MainWindow::onLogMessage);

    // 工作流错误信号 - 显示错误消息
    connect(m_workflow, &WorkflowEngine::errorOccurred,
            this, [this](const QString &err) {
                appendLog(tr("[错误] %1").arg(err));
            });

    // ============ 激光传感器信号连接 ============
    // 数据接收信号
    connect(m_laserSensor, &SerialManager::dataReceived,
            this, &MainWindow::onLaserDataReceived);

    // 连接状态改变信号
    connect(m_laserSensor, &SerialManager::connectionChanged,
            this, [this](bool connected) {
                Q_UNUSED(connected)
                updateConnectionStatus();
            });

    // ============ FMC4030轴控制器信号连接 ============
    // 轴位置和状态更新信号
    connect(m_axisController, &AxisController::statusUpdated,
            this, &MainWindow::onAxisStatusUpdated);

    // 轴状态改变信号 - 更新状态标签显示
    connect(m_axisController, &AxisController::stateChanged,
            this, [this](AxisController::AxisState state) {
                QString stateText;
                switch (state) {
                case AxisController::Idle:        stateText = tr("空闲"); break;
                case AxisController::Moving:      stateText = tr("运动中"); break;
                case AxisController::Homing:      stateText = tr("回零中"); break;
                case AxisController::Error:       stateText = tr("错误"); break;
                case AxisController::Disconnected: stateText = tr("未连接"); break;
                default:                          stateText = tr("未知"); break;
                }
                ui->axisStateLabel->setText(stateText);
            });

    // ============ 液态镜头信号连接 ============
    // 焦距值改变信号
    connect(m_lensController, &LiquidLensController::focusChanged,
            this, [this](int val) {
                ui->lensFocusValueLabel->setText(QString::number(val));
                ui->focusDisplay->setText(QString::number(val));
            });
}

/**
 * @brief 刷新串口列表
 *
 * 扫描系统中可用的串口，更新到下拉列表中
 */
void MainWindow::onRefreshPorts()
{
    updatePortComboBoxes();
}

/**
 * @brief 连接激光传感器
 *
 * 从UI获取串口参数，尝试打开串口连接
 */
void MainWindow::onConnectLaser()
{
    // 获取用户选择的串口
    QString port = ui->laserPortCombo->currentText();

    // 检查是否选择了串口
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择激光传感器串口"));
        return;
    }

    // 获取用户选择的波特率
    qint32 baud = ui->laserBaudCombo->currentText().toInt();

    // 尝试打开串口
    if (m_laserSensor->open(port, baud)) {
        appendLog(tr("激光传感器已连接: %1 @ %2 bps").arg(port).arg(baud));
    }

    // 更新连接状态显示
    updateConnectionStatus();
}

/**
 * @brief 断开激光传感器
 */
void MainWindow::onDisconnectLaser()
{
    // 关闭串口连接
    m_laserSensor->close();

    // 记录日志
    appendLog(tr("激光传感器已断开"));

    // 更新连接状态显示
    updateConnectionStatus();
}

/**
 * @brief 连接液态镜头
 *
 * 从UI获取串口参数，尝试打开串口连接
 */
void MainWindow::onConnectLens()
{
    // 获取用户选择的串口
    QString port = ui->lensPortCombo->currentText();

    // 检查是否选择了串口
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择液态镜头串口"));
        return;
    }

    // 获取用户选择的波特率
    qint32 baud = ui->lensBaudCombo->currentText().toInt();

    // 尝试打开串口
    if (m_lensController->connectLens(port, baud)) {
        appendLog(tr("液态镜头已连接: %1 @ %2 bps").arg(port).arg(baud));
    }

    // 更新连接状态显示
    updateConnectionStatus();
}

/**
 * @brief 断开液态镜头
 */
void MainWindow::onDisconnectLens()
{
    // 关闭串口连接
    m_lensController->disconnectLens();

    // 记录日志
    appendLog(tr("液态镜头已断开"));

    // 更新连接状态显示
    updateConnectionStatus();
}

/**
 * @brief 连接FMC4030运动轴
 *
 * 从UI获取IP地址和端口，尝试通过网口连接控制器
 */
void MainWindow::onConnectAxis()
{
    // 获取用户输入的IP地址
    QString ip = ui->axisIpEdit->text().trimmed();

    // 获取用户设置的端口号
    int port = ui->axisPortSpin->value();

    // 检查IP地址是否为空
    if (ip.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入FMC4030控制器IP地址"));
        return;
    }

    // 尝试连接控制器
    if (m_axisController->connectAxis(ip, port)) {
        appendLog(tr("FMC4030已连接: %1:%2").arg(ip).arg(port));
    } else {
        // 连接失败提示
        QMessageBox::warning(this, tr("错误"), tr("无法连接到FMC4030控制器"));
    }

    // 更新连接状态显示
    updateConnectionStatus();
}

/**
 * @brief 断开FMC4030运动轴
 */
void MainWindow::onDisconnectAxis()
{
    // 断开网络连接
    m_axisController->disconnectAxis();

    // 记录日志
    appendLog(tr("FMC4030已断开"));

    // 更新连接状态显示
    updateConnectionStatus();
}

/**
 * @brief 轴移动到指定位置
 *
 * 获取用户设置的轴和目标位置，执行移动命令
 */
void MainWindow::onAxisMoveTo()
{
    // 获取目标位置
    double pos = ui->axisPosSpin->value();

    // 获取选中的轴 (0=X, 1=Y, 2=Z)
    int axis = ui->axisSelectCombo->currentIndex();

    // 发送移动命令
    m_axisController->moveToPosition(pos, axis);

    // 记录日志
    appendLog(tr("轴%1移动到 %2 mm").arg(axis).arg(pos, 0, 'f', 2));
}

/**
 * @brief 轴回零
 *
 * 将选中的轴移动到机械原点位置
 */
void MainWindow::onAxisHome()
{
    // 获取选中的轴
    int axis = ui->axisSelectCombo->currentIndex();

    // 发送回零命令
    m_axisController->home(axis);

    // 记录日志
    appendLog(tr("轴%1回零中...").arg(axis));
}

/**
 * @brief 轴停止运动
 *
 * 立即停止选中的轴的运动
 */
void MainWindow::onAxisStop()
{
    // 获取选中的轴
    int axis = ui->axisSelectCombo->currentIndex();

    // 发送停止命令
    m_axisController->stop(axis);

    // 记录日志
    appendLog(tr("轴%1已停止").arg(axis));
}

/**
 * @brief 轴速度改变
 *
 * @param value 新的速度值 (mm/s)
 */
void MainWindow::onAxisSpeedChanged(int value)
{
    // 更新速度标签显示
    ui->axisSpeedLabel->setText(QString("%1 mm/s").arg(value));

    // 设置控制器速度
    m_axisController->setSpeed(value);
}

/**
 * @brief 焦距值改变
 *
 * @param value 新的焦距值 (0-1023)
 */
void MainWindow::onLensFocusChanged(int value)
{
    // 直接设置焦距值
    m_lensController->setFocusDirect(value);
}

/**
 * @brief 添加标定点
 *
 * 弹出对话框，让用户输入距离和对应的焦距值，
 * 将该点添加到标定曲线中
 */
void MainWindow::onLensCalibrate()
{
    bool ok1 = false, ok2 = false;

    // 获取距离值
    double dist = QInputDialog::getDouble(
        this, tr("标定"), tr("距离 (mm):"),
        100, 0, 9999, 2, &ok1);

    // 用户取消操作
    if (!ok1) return;

    // 获取焦距值
    int focus = QInputDialog::getInt(
        this, tr("标定"), tr("焦距值 (0-1023):"),
        512, 0, 1023, 1, &ok2);

    // 用户取消操作
    if (!ok2) return;

    // 添加标定点
    m_lensController->addCalibrationPoint(dist, focus);

    // 记录日志
    appendLog(tr("添加标定点: 距离=%1mm, 焦距=%2").arg(dist, 0, 'f', 1).arg(focus));
}

void MainWindow::onLensOffsetChanged(double value)
{
    m_lensController->setOffset(value);
    appendLog(tr("传感器偏移量设置为: %1 mm").arg(value, 0, 'f', 2));
}

void MainWindow::onLensFreqChanged(int value)
{
    m_workflow->setLensAdjustInterval(value);
}

/**
 * @brief 单点测量
 *
 * 在当前位置触发一次距离测量，
 * 并根据测量结果调整镜头焦距
 */
void MainWindow::onSingleMeasure()
{
    // 启动工作流的单点测量模式
    m_workflow->startSinglePoint();
}

/**
 * @brief 开始连续扫描
 *
 * 根据UI设置的起始位置、终止位置和步进值，
 * 执行多点连续扫描
 */
void MainWindow::onStartScan()
{
    // 获取扫描参数
    double start = ui->scanStartSpin->value();   // 起始位置
    double end = ui->scanEndSpin->value();       // 终止位置
    double step = ui->scanStepSpin->value();     // 步进值

    // 检查步进值是否有效
    if (step <= 0) {
        QMessageBox::warning(this, tr("警告"), tr("步进值必须大于0"));
        return;
    }

    // 启动连续扫描
    m_workflow->startContinuousScan(start, end, step);
}

/**
 * @brief 暂停扫描
 *
 * 暂停当前扫描过程和轴运动
 */
void MainWindow::onPauseScan()
{
    // 暂停工作流
    m_workflow->pause();

    // 暂停轴运动
    m_axisController->pause();
}

/**
 * @brief 继续扫描
 *
 * 从暂停状态恢复扫描
 */
void MainWindow::onResumeScan()
{
    // 恢复工作流
    m_workflow->resume();

    // 恢复轴运动
    m_axisController->resume();
}

/**
 * @brief 停止扫描
 *
 * 完全停止当前扫描过程
 */
void MainWindow::onStopScan()
{
    // 停止工作流
    m_workflow->stop();

    // 停止所有轴运动
    m_axisController->stopAll();
}

/**
 * @brief 工作流状态改变
 *
 * @param state 新的工作流状态
 */
void MainWindow::onWorkflowStateChanged(WorkflowEngine::WorkflowState state)
{
    QString stateStr;

    // 根据状态显示对应的中文文本
    switch (state) {
    case WorkflowEngine::Stopped:       stateStr = tr("停止"); break;
    case WorkflowEngine::Initializing:   stateStr = tr("初始化"); break;
    case WorkflowEngine::WaitingForAxis: stateStr = tr("等待轴到位"); break;
    case WorkflowEngine::Measuring:    stateStr = tr("测量中"); break;
    case WorkflowEngine::AdjustingFocus: stateStr = tr("调焦中"); break;
    case WorkflowEngine::Paused:        stateStr = tr("已暂停"); break;
    case WorkflowEngine::Error:         stateStr = tr("错误"); break;
    }

    // 更新状态标签
    ui->workflowStateLabel->setText(stateStr);
}

/**
 * @brief 距离测量完成
 *
 * @param distance 测量到的距离值（毫米）
 * @param focusValue 对应的焦距值
 */
void MainWindow::onDistanceMeasured(double distance, int focusValue)
{
    // 更新距离显示
    ui->distanceDisplay->setText(QString("%1 mm").arg(distance, 0, 'f', 2));

    // 更新焦距显示
    ui->focusDisplay->setText(QString::number(focusValue));
}

/**
 * @brief 扫描点完成
 *
 * @param point 扫描点数据，包含位置、距离和焦距
 */
void MainWindow::onScanPointCompleted(const WorkflowEngine::ScanPoint &point)
{
    // 在结果表格中添加一行
    int row = ui->resultTable->rowCount();
    ui->resultTable->insertRow(row);

    // 填充表格数据
    ui->resultTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));          // 序号
    ui->resultTable->setItem(row, 1, new QTableWidgetItem(QString("%1").arg(point.position, 0, 'f', 2)));  // 轴位置
    ui->resultTable->setItem(row, 2, new QTableWidgetItem(QString("%1").arg(point.measuredDistance, 0, 'f', 2)));  // 距离
    ui->resultTable->setItem(row, 3, new QTableWidgetItem(QString::number(point.focusValue)));   // 焦距

    // 滚动到最新行
    ui->resultTable->scrollToBottom();
}

/**
 * @brief 扫描完成
 */
void MainWindow::onScanFinished()
{
    // 记录日志
    appendLog(tr("扫描完成"));
}

/**
 * @brief 进度改变
 *
 * @param current 当前完成的点数
 * @param total 总共的点数
 */
void MainWindow::onProgressChanged(int current, int total)
{
    // 设置进度条范围
    ui->scanProgressBar->setMaximum(total);

    // 更新当前进度
    ui->scanProgressBar->setValue(current);
}

/**
 * @brief 日志消息
 *
 * @param msg 日志内容
 */
void MainWindow::onLogMessage(const QString &msg)
{
    // 添加日志到日志文本框
    appendLog(msg);
}

/**
 * @brief 激光传感器数据接收
 *
 * @param data 接收到的原始数据
 */
void MainWindow::onLaserDataReceived(const QByteArray &data)
{
    // 转换数据为字符串并去除空白
    QString text = QString::fromUtf8(data).trimmed();

    // 确保数据不为空
    if (!text.isEmpty()) {
        // 尝试解析为数值
        bool ok = false;
        double val = text.toDouble(&ok);

        // 解析成功且为有效距离值
        if (ok && val >= 0) {
            ui->distanceDisplay->setText(QString("%1 mm").arg(val, 0, 'f', 2));
        }
    }
}

/**
 * @brief 轴状态更新
 *
 * @param x X轴当前位置
 * @param y Y轴当前位置
 * @param z Z轴当前位置
 */
void MainWindow::onAxisStatusUpdated(float x, float y, float z)
{
    // 更新三个轴的位置显示
    ui->axisXDisplay->setText(QString("X: %1").arg(x, 0, 'f', 2));
    ui->axisYDisplay->setText(QString("Y: %1").arg(y, 0, 'f', 2));
    ui->axisZDisplay->setText(QString("Z: %1").arg(z, 0, 'f', 2));
}

/**
 * @brief 更新串口下拉列表
 *
 * 扫描系统中可用的串口，更新到激光传感器和液态镜头的下拉列表
 */
void MainWindow::updatePortComboBoxes()
{
    // 获取可用串口列表
    QStringList ports = SerialManager::availablePorts();

    // 清空并重新填充下拉列表
    ui->laserPortCombo->clear();
    ui->lensPortCombo->clear();

    ui->laserPortCombo->addItems(ports);
    ui->lensPortCombo->addItems(ports);
}

/**
 * @brief 更新设备连接状态显示
 *
 * 根据各设备的实际连接状态更新UI标签的颜色和文本
 */
void MainWindow::updateConnectionStatus()
{
    // ============ 激光传感器状态 ============
    if (m_laserSensor->isOpen()) {
        // 已连接 - 绿色显示
        ui->laserStatusLabel->setText(tr("已连接"));
        ui->laserStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        // 未连接 - 红色显示
        ui->laserStatusLabel->setText(tr("未连接"));
        ui->laserStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    // ============ 液态镜头状态 ============
    if (m_lensController->isConnected()) {
        // 已连接 - 绿色显示
        ui->lensStatusLabel->setText(tr("已连接"));
        ui->lensStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        // 未连接 - 红色显示
        ui->lensStatusLabel->setText(tr("未连接"));
        ui->lensStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    // ============ FMC4030状态 ============
    if (m_axisController->isConnected()) {
        // 已连接 - 绿色显示
        ui->axisStatusLabel->setText(tr("已连接"));
        ui->axisStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        // 未连接 - 红色显示
        ui->axisStatusLabel->setText(tr("未连接"));
        ui->axisStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

/**
 * @brief 添加日志信息
 *
 * @param msg 日志消息内容
 *
 * 日志格式: [时间戳] 消息内容
 */
void MainWindow::appendLog(const QString &msg)
{
    // 获取当前时间戳
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    // 添加带时间戳的日志行
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, msg));
}
