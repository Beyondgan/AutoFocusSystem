/**
 * @file liquidlenscontroller.cpp
 * @brief 液态镜头控制器实现文件
 */

#include "liquidlenscontroller.h"
#include <QDebug>
#include <QtMath>

/**
 * @brief 构造函数
 * @param parent 父对象指针
 */
LiquidLensController::LiquidLensController(QObject *parent)
    : QObject(parent)
    , m_currentFocus(0)
    , m_currentDistance(0.0)
    , m_minFocus(0)
    , m_maxFocus(1023)
{
    // 创建串口管理器
    m_serial = new SerialManager(SerialManager::LiquidLens, this);

    // 连接信号槽
    connect(m_serial, &SerialManager::dataReceived,
            this, &LiquidLensController::onLensDataReceived);
    connect(m_serial, &SerialManager::errorOccurred,
            this, &LiquidLensController::onLensError);
}

/**
 * @brief 析构函数
 */
LiquidLensController::~LiquidLensController()
{
    disconnectLens();
}

/**
 * @brief 连接到液态镜头
 * @param portName 串口名称
 * @param baudRate 波特率
 * @return 连接是否成功
 */
bool LiquidLensController::connectLens(const QString &portName, qint32 baudRate)
{
    return m_serial->open(portName, baudRate);
}

/**
 * @brief 断开连接
 */
void LiquidLensController::disconnectLens()
{
    m_serial->close();
}

/**
 * @brief 检查连接状态
 * @return 是否已连接
 */
bool LiquidLensController::isConnected() const
{
    return m_serial->isOpen();
}

/**
 * @brief 根据距离设置焦距
 * @param distanceMm 目标距离（毫米）
 */
void LiquidLensController::setFocusByDistance(double distanceMm)
{
    // 保存当前距离
    m_currentDistance = distanceMm;

    // 根据距离计算焦距值
    int focus = distanceToFocus(distanceMm);

    // 设置焦距
    setFocusDirect(focus);

    // 发送距离更新信号
    emit distanceUpdated(distanceMm);
}

/**
 * @brief 直接设置焦距值
 * @param focusValue 焦距值
 */
void LiquidLensController::setFocusDirect(int focusValue)
{
    // 限制焦距值在有效范围内
    focusValue = qBound(m_minFocus, focusValue, m_maxFocus);

    // 更新当前焦距值
    m_currentFocus = focusValue;

    // 构建并发送控制命令
    QByteArray cmd = buildLensCommand(focusValue);
    if (m_serial->sendData(cmd)) {
        emit focusChanged(focusValue);
    }
}

/**
 * @brief 获取当前焦距值
 * @return 当前焦距值
 */
int LiquidLensController::currentFocusValue() const
{
    return m_currentFocus;
}

/**
 * @brief 获取当前距离
 * @return 当前距离值
 */
double LiquidLensController::currentDistance() const
{
    return m_currentDistance;
}

/**
 * @brief 添加标定点
 * @param distanceMm 距离值（毫米）
 * @param focusValue 焦距值
 */
void LiquidLensController::addCalibrationPoint(double distanceMm, int focusValue)
{
    m_calibration[distanceMm] = focusValue;
}

/**
 * @brief 清除所有标定点
 */
void LiquidLensController::clearCalibrationPoints()
{
    m_calibration.clear();
}

/**
 * @brief 加载标定曲线
 * @param points 标定点映射
 */
void LiquidLensController::loadCalibration(const QMap<double, int> &points)
{
    m_calibration = points;
}

/**
 * @brief 设置最小焦距值
 * @param min 最小值
 */
void LiquidLensController::setMinFocus(int min)
{
    m_minFocus = min;
}

/**
 * @brief 设置最大焦距值
 * @param max 最大值
 */
void LiquidLensController::setMaxFocus(int max)
{
    m_maxFocus = max;
}

/**
 * @brief 获取最小焦距值
 * @return 最小值
 */
int LiquidLensController::minFocus() const
{
    return m_minFocus;
}

/**
 * @brief 获取最大焦距值
 * @return 最大值
 */
int LiquidLensController::maxFocus() const
{
    return m_maxFocus;
}

/**
 * @brief 将距离转换为焦距值
 * @param distanceMm 距离值
 * @return 焦距值
 */
int LiquidLensController::distanceToFocus(double distanceMm) const
{
    return interpolateFocus(distanceMm);
}

/**
 * @brief 插值计算焦距
 * @param distanceMm 距离值
 * @return 焦距值
 */
int LiquidLensController::interpolateFocus(double distanceMm) const
{
    // 如果没有标定点，使用线性映射
    if (m_calibration.isEmpty()) {
        double ratio = qBound(0.0, (distanceMm - 50.0) / 950.0, 1.0);
        return static_cast<int>(m_minFocus + ratio * (m_maxFocus - m_minFocus));
    }

    // 只有一个标定点
    if (m_calibration.size() == 1) {
        return m_calibration.first();
    }

    // 找到距离值在标定曲线中的位置
    auto upper = m_calibration.lowerBound(distanceMm);

    // 超出最大距离
    if (upper == m_calibration.end()) {
        return m_calibration.last();
    }

    // 小于最小距离
    if (upper == m_calibration.begin()) {
        return m_calibration.first();
    }

    // 获取相邻的两个标定点
    auto lower = upper - 1;
    double dLower = lower.key();
    double dUpper = upper.key();
    int fLower = lower.value();
    int fUpper = upper.value();

    // 防止除零
    if (qFuzzyCompare(dLower, dUpper)) {
        return fLower;
    }

    // 线性插值
    double ratio = (distanceMm - dLower) / (dUpper - dLower);
    return static_cast<int>(fLower + ratio * (fUpper - fLower));
}

/**
 * @brief 数据接收处理
 * @param data 接收到的数据
 */
void LiquidLensController::onLensDataReceived(const QByteArray &data)
{
    Q_UNUSED(data)
}

/**
 * @brief 错误处理
 * @param error 错误描述
 */
void LiquidLensController::onLensError(const QString &error)
{
    emit errorOccurred(error);
}

/**
 * @brief 构建镜头控制命令
 * @param focusValue 焦距值
 * @return 命令字节数组
 */
QByteArray LiquidLensController::buildLensCommand(int focusValue)
{
    return QString("FOCUS %1\r\n").arg(focusValue).toUtf8();
}
