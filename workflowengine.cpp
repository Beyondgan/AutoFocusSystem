/**
 * @file workflowengine.cpp
 * @brief 工作流引擎实现文件
 *
 * 实现自动对焦系统的核心工作流程：
 *   - 单点测量模式
 *   - 连续扫描模式
 *   - 状态管理和控制
 */

#include "workflowengine.h"
#include <QDebug>
#include <QRegularExpression>

/**
 * @brief 构造函数
 * @param parent 父对象指针
 *
 * 初始化成员变量和测量定时器
 */
WorkflowEngine::WorkflowEngine(QObject *parent)
    : QObject(parent)
    , m_laserSensor(nullptr)
    , m_axis(nullptr)
    , m_lens(nullptr)
    , m_state(Stopped)
    , m_currentIndex(0)
    , m_totalPoints(0)
    , m_measureDelay(200)
    , m_autoFocusEnabled(true)
    , m_continuousMode(false)
    , m_lastDistance(0.0)
    , m_currentAxisPos(0.0)
    , m_scanAxis(0)
    , m_pendingDistance(0.0)
    , m_lensAdjustInterval(0)
    , m_latestSensorDistance(0.0)
{
    m_measureTimer = new QTimer(this);
    m_measureTimer->setSingleShot(true);
    connect(m_measureTimer, &QTimer::timeout, this, &WorkflowEngine::onMeasureTimeout);

    m_lensAdjustTimer = new QTimer(this);
    m_lensAdjustTimer->setSingleShot(true);
    connect(m_lensAdjustTimer, &QTimer::timeout, this, &WorkflowEngine::onLensAdjustTimeout);
}

/**
 * @brief 析构函数
 */
WorkflowEngine::~WorkflowEngine()
{
    stop();
}

/**
 * @brief 设置激光传感器
 * @param sensor 传感器指针
 */
void WorkflowEngine::setLaserSensor(SerialManager *sensor)
{
    // 断开旧的连接
    if (m_laserSensor) {
        disconnect(m_laserSensor, nullptr, this, nullptr);
    }

    m_laserSensor = sensor;

    // 连接新的信号
    if (m_laserSensor) {
        connect(m_laserSensor, &SerialManager::dataReceived,
                this, &WorkflowEngine::onSensorDataReceived);
    }
}

/**
 * @brief 设置轴控制器
 * @param axis 轴控制器指针
 */
void WorkflowEngine::setAxisController(AxisController *axis)
{
    // 断开旧的连接
    if (m_axis) {
        disconnect(m_axis, nullptr, this, nullptr);
    }

    m_axis = axis;

    // 连接新的信号
    if (m_axis) {
        connect(m_axis, &AxisController::moveFinished,
                this, &WorkflowEngine::onAxisMoveFinished);
        connect(m_axis, &AxisController::positionChanged,
                this, &WorkflowEngine::onAxisPositionChanged);
    }
}

/**
 * @brief 设置液态镜头控制器
 * @param lens 镜头控制器指针
 */
void WorkflowEngine::setLiquidLens(LiquidLensController *lens)
{
    m_lens = lens;
}

/**
 * @brief 开始单点测量
 *
 * 在当前位置触发一次距离测量，
 * 并根据测量结果调整镜头焦距
 */
void WorkflowEngine::startSinglePoint()
{
    // 检查激光传感器是否连接
    if (!m_laserSensor || !m_laserSensor->isOpen()) {
        emit errorOccurred(tr("激光传感器未连接"));
        return;
    }

    // 设置为非连续模式
    m_continuousMode = false;
    m_scanPositions.clear();

    // 切换到测量状态
    setState(Measuring);
    emit logMessage(tr("单点测量开始"));

    // 发送测量请求
    requestMeasurement();
}

/**
 * @brief 开始连续扫描
 * @param startPos 起始位置
 * @param endPos 终止位置
 * @param stepSize 步进值
 */
void WorkflowEngine::startContinuousScan(double startPos, double endPos, double stepSize)
{
    // 生成位置列表
    QList<double> positions;

    // 检查步进值
    if (stepSize <= 0) {
        emit errorOccurred(tr("步进值必须大于0"));
        return;
    }

    // 根据方向生成位置序列
    double pos = startPos;
    if (startPos <= endPos) {
        // 正向
        while (pos <= endPos + stepSize * 0.01) {
            positions.append(pos);
            pos += stepSize;
        }
    } else {
        // 反向
        while (pos >= endPos - stepSize * 0.01) {
            positions.append(pos);
            pos -= stepSize;
        }
    }

    // 调用重载函数开始扫描
    startContinuousScan(positions);
}

/**
 * @brief 开始连续扫描（使用位置列表）
 * @param positions 位置列表
 */
void WorkflowEngine::startContinuousScan(const QList<double> &positions)
{
    // 检查激光传感器
    if (!m_laserSensor || !m_laserSensor->isOpen()) {
        emit errorOccurred(tr("激光传感器未连接"));
        return;
    }

    // 检查轴控制器
    if (!m_axis || !m_axis->isConnected()) {
        emit errorOccurred(tr("FMC4030控制器未连接"));
        return;
    }

    // 检查位置列表
    if (positions.isEmpty()) {
        emit errorOccurred(tr("扫描位置列表为空"));
        return;
    }

    // 设置为连续扫描模式
    m_continuousMode = true;
    m_scanPositions.clear();

    // 将位置添加到队列
    for (double pos : positions) {
        m_scanPositions.enqueue(pos);
    }

    // 设置总点数和当前索引
    m_totalPoints = m_scanPositions.size();
    m_currentIndex = 0;

    // 清空结果列表
    m_results.clear();

    // 切换到等待轴状态
    setState(WaitingForAxis);

    // 发送日志
    emit logMessage(tr("连续扫描开始，共 %1 个点位").arg(m_totalPoints));

    // 处理第一个点
    processNextPoint();
}

/**
 * @brief 暂停工作流
 */
void WorkflowEngine::pause()
{
    // 只有在特定状态才能暂停
    if (m_state == WaitingForAxis || m_state == Measuring || m_state == AdjustingFocus) {
        // 停止测量定时器
        m_measureTimer->stop();

        // 暂停轴运动
        if (m_axis) m_axis->pause();

        // 切换到暂停状态
        setState(Paused);

        emit logMessage(tr("工作流已暂停"));
    }
}

/**
 * @brief 恢复工作流
 */
void WorkflowEngine::resume()
{
    if (m_state == Paused) {
        setState(WaitingForAxis);
        emit logMessage(tr("工作流已恢复"));
        processNextPoint();
    }
}

/**
 * @brief 停止工作流
 */
void WorkflowEngine::stop()
{
    // 停止测量定时器
    m_measureTimer->stop();

    // 清空位置队列
    m_scanPositions.clear();

    // 停止所有轴运动
    if (m_axis) m_axis->stopAll();

    // 切换到停止状态
    setState(Stopped);

    emit logMessage(tr("工作流已停止"));
}

/**
 * @brief 获取当前状态
 * @return 当前工作流状态
 */
WorkflowEngine::WorkflowState WorkflowEngine::state() const
{
    return m_state;
}

/**
 * @brief 获取扫描结果
 * @return 扫描点列表
 */
QList<WorkflowEngine::ScanPoint> WorkflowEngine::scanResults() const
{
    return m_results;
}

/**
 * @brief 清除扫描结果
 */
void WorkflowEngine::clearResults()
{
    m_results.clear();
}

/**
 * @brief 设置测量延迟
 * @param msec 延迟时间（毫秒）
 */
void WorkflowEngine::setMeasureDelay(int msec)
{
    m_measureDelay = msec;
}

/**
 * @brief 设置是否启用自动对焦
 * @param enabled 是否启用
 */
void WorkflowEngine::setAutoFocusEnabled(bool enabled)
{
    m_autoFocusEnabled = enabled;
}

/**
 * @brief 设置扫描轴
 * @param axis 轴序号（0=X, 1=Y, 2=Z）
 */
void WorkflowEngine::setScanAxis(int axis)
{
    m_scanAxis = axis;
}

void WorkflowEngine::setLensAdjustInterval(int intervalMs)
{
    m_lensAdjustInterval = intervalMs;
    if (intervalMs > 0) {
        emit logMessage(tr("镜头调节间隔设置为: %1 ms (频率: %2 Hz)")
            .arg(intervalMs)
            .arg(1000.0 / intervalMs, 0, 'f', 1));
    } else {
        emit logMessage(tr("镜头调节间隔设置为: 全速调节"));
    }
}

int WorkflowEngine::lensAdjustInterval() const
{
    return m_lensAdjustInterval;
}

void WorkflowEngine::onLensAdjustTimeout()
{
    if (m_state != Measuring && m_state != AdjustingFocus) return;

    if (!m_autoFocusEnabled || !m_lens || !m_lens->isConnected()) return;

    double offset = m_lens->offset();
    double actualDistance = m_latestSensorDistance + offset;
    int focusValue = m_lens->currentFocusValue();

    emit logMessage(tr("【节流】传感器距离: %1 mm, 实际距离: %2 mm, 焦距值: %3")
        .arg(m_latestSensorDistance, 0, 'f', 2)
        .arg(actualDistance, 0, 'f', 2)
        .arg(focusValue));

    if (m_continuousMode) {
        setState(WaitingForAxis);
        processNextPoint();
    }
}

/**
 * @brief 传感器数据接收处理
 * @param data 接收到的数据
 *
 * 解析距离数据，并触发自动对焦
 */
void WorkflowEngine::onSensorDataReceived(const QByteArray &data)
{
    if (m_state != Measuring) return;

    double distance = parseDistance(data);
    if (distance < 0) return;

    m_lastDistance = distance;
    m_pendingDistance = distance;
    m_latestSensorDistance = distance;

    if (m_autoFocusEnabled && m_lens && m_lens->isConnected()) {
        if (m_lensAdjustInterval > 0) {
            if (!m_lensAdjustTimer->isActive()) {
                double offset = m_lens->offset();
                double actualDistance = distance + offset;
                m_lens->setFocusByDistance(actualDistance);
                int focusValue = m_lens->currentFocusValue();
                emit logMessage(tr("传感器距离: %1 mm, 轴位置: %2 mm, 偏移: %3 mm, 实际距离: %4 mm, 焦距值: %5")
                    .arg(distance, 0, 'f', 2)
                    .arg(m_currentAxisPos, 0, 'f', 2)
                    .arg(offset, 0, 'f', 2)
                    .arg(actualDistance, 0, 'f', 2)
                    .arg(focusValue));
                m_lensAdjustTimer->start(m_lensAdjustInterval);
            } else {
                emit logMessage(tr("【跳过】传感器距离: %1 mm (等待%2ms)")
                    .arg(distance, 0, 'f', 2)
                    .arg(m_lensAdjustInterval));
            }
        } else {
            double offset = m_lens->offset();
            double actualDistance = distance + offset;
            m_lens->setFocusByDistance(actualDistance);
            int focusValue = m_lens->currentFocusValue();
            emit logMessage(tr("传感器距离: %1 mm, 轴位置: %2 mm, 偏移: %3 mm, 实际距离: %4 mm, 焦距值: %5")
                .arg(distance, 0, 'f', 2)
                .arg(m_currentAxisPos, 0, 'f', 2)
                .arg(offset, 0, 'f', 2)
                .arg(actualDistance, 0, 'f', 2)
                .arg(focusValue));
        }
    }

    emit distanceMeasured(distance, m_autoFocusEnabled && m_lens ? m_lens->currentFocusValue() : 0);

    if (m_continuousMode) {
        ScanPoint point;
        point.position = m_currentAxisPos;
        point.measuredDistance = distance;
        point.focusValue = m_lens ? m_lens->currentFocusValue() : 0;
        m_results.append(point);

        emit scanPointCompleted(point);

        m_currentIndex++;
        emit progressChanged(m_currentIndex, m_totalPoints);

        if (m_scanPositions.isEmpty()) {
            setState(Stopped);
            emit scanFinished();
            emit logMessage(tr("连续扫描完成，共 %1 个点位").arg(m_results.size()));
        } else {
            if (m_lensAdjustInterval <= 0) {
                setState(WaitingForAxis);
                processNextPoint();
            }
        }
    } else {
        ScanPoint point;
        point.position = m_currentAxisPos;
        point.measuredDistance = distance;
        point.focusValue = m_lens ? m_lens->currentFocusValue() : 0;
        m_results.append(point);

        emit scanPointCompleted(point);

        setState(Stopped);
        emit logMessage(tr("单点测量完成"));
    }
}

/**
 * @brief 轴运动完成处理
 * @param success 是否成功
 * @param axis 轴序号
 */
void WorkflowEngine::onAxisMoveFinished(bool success, int axis)
{
    Q_UNUSED(axis)

    // 只在等待轴状态处理
    if (m_state != WaitingForAxis) return;

    // 检查运动是否成功
    if (!success) {
        setState(Error);
        emit errorOccurred(tr("轴运动失败"));
        return;
    }

    // 发送日志
    emit logMessage(tr("轴运动到位，开始测量"));

    // 切换到测量状态
    setState(Measuring);

    // 发送测量请求
    requestMeasurement();
}

/**
 * @brief 轴位置改变处理
 * @param position 当前位置
 * @param axis 轴序号
 */
void WorkflowEngine::onAxisPositionChanged(double position, int axis)
{
    Q_UNUSED(axis)
    m_currentAxisPos = position;
}

/**
 * @brief 测量超时处理
 */
void WorkflowEngine::onMeasureTimeout()
{
    // 只在测量状态处理
    if (m_state != Measuring) return;

    // 发送超时日志
    emit logMessage(tr("测量超时，重试中..."));

    // 重试测量
    requestMeasurement();
}

/**
 * @brief 设置工作流状态
 * @param state 新状态
 */
void WorkflowEngine::setState(WorkflowState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

/**
 * @brief 处理下一个扫描点
 */
void WorkflowEngine::processNextPoint()
{
    // 队列为空则返回
    if (m_scanPositions.isEmpty()) return;

    // 取出下一个位置
    double targetPos = m_scanPositions.dequeue();

    // 发送日志
    emit logMessage(tr("移动到位置: %1 mm").arg(targetPos, 0, 'f', 2));

    // 发送轴运动命令
    if (m_axis) {
        m_axis->moveToPosition(targetPos, m_scanAxis);
    }
}

/**
 * @brief 解析距离值
 * @param data 原始数据
 * @return 距离值，解析失败返回-1
 */
double WorkflowEngine::parseDistance(const QByteArray &data)
{
    // 转换为字符串
    QString str = QString::fromUtf8(data).trimmed();

    // 使用正则表达式提取数字
    QRegularExpression re(R"([+-]?\d+\.?\d*)");
    QRegularExpressionMatch match = re.match(str);

    // 找到匹配则解析为数值
    if (match.hasMatch()) {
        bool ok = false;
        double val = match.captured(0).toDouble(&ok);
        if (ok && val >= 0) return val;
    }

    return -1.0;
}

/**
 * @brief 请求测量
 *
 * 向激光传感器发送测量命令
 */
void WorkflowEngine::requestMeasurement()
{
    // 检查传感器是否连接
    if (m_laserSensor && m_laserSensor->isOpen()) {
        // 发送测量命令
        QByteArray cmd = "MEASURE\r\n";
        m_laserSensor->sendData(cmd);

        // 启动超时定时器
        m_measureTimer->start(m_measureDelay * 5);
    }
}
