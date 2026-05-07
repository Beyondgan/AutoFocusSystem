/**
 * @file serialmanager.cpp
 * @brief 串口管理器实现文件
 */

#include "serialmanager.h"
#include <QDebug>

/**
 * @brief 构造函数
 * @param type 设备类型
 * @param parent 父对象指针
 *
 * 创建串口对象并连接信号槽
 */
SerialManager::SerialManager(DeviceType type, QObject *parent)
    : QObject(parent)
    , m_deviceType(type)
    , m_readTimeout(100)
{
    // 创建串口对象
    m_serial = new QSerialPort(this);

    // 连接数据接收信号
    connect(m_serial, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);

    // 连接错误信号
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialManager::onError);
}

/**
 * @brief 析构函数
 *
 * 关闭串口连接，释放资源
 */
SerialManager::~SerialManager()
{
    close();
}

/**
 * @brief 获取系统中可用的串口列表
 * @return 可用串口名称列表（如 "COM1", "COM2" 等）
 */
QStringList SerialManager::availablePorts()
{
    QStringList list;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        list << info.portName();
    }
    return list;
}

/**
 * @brief 打开串口连接
 * @param portName 串口名称
 * @param baudRate 波特率
 * @param dataBits 数据位
 * @param parity 校验位
 * @param stopBits 停止位
 * @return 是否成功打开
 */
bool SerialManager::open(const QString &portName, qint32 baudRate,
                          QSerialPort::DataBits dataBits,
                          QSerialPort::Parity parity,
                          QSerialPort::StopBits stopBits)
{
    // 如果串口已打开，先关闭
    close();

    // 设置串口参数
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(dataBits);
    m_serial->setParity(parity);
    m_serial->setStopBits(stopBits);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // 尝试打开串口（读写模式）
    if (m_serial->open(QIODevice::ReadWrite)) {
        // 清空缓冲区
        m_buffer.clear();
        // 发送连接状态改变信号
        emit connectionChanged(true);
        return true;
    }

    // 打开失败，发送错误信号
    emit errorOccurred(tr("无法打开串口 %1: %2").arg(portName, m_serial->errorString()));
    return false;
}

/**
 * @brief 关闭串口连接
 */
void SerialManager::close()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);
    }
}

/**
 * @brief 检查串口是否打开
 * @return 是否已打开
 */
bool SerialManager::isOpen() const
{
    return m_serial->isOpen();
}

/**
 * @brief 发送数据
 * @param data 要发送的数据
 * @return 发送是否成功
 */
bool SerialManager::sendData(const QByteArray &data)
{
    // 检查串口是否打开
    if (!m_serial->isOpen()) {
        emit errorOccurred(tr("串口未打开，无法发送数据"));
        return false;
    }

    // 写入数据
    qint64 written = m_serial->write(data);

    // 检查写入是否完整
    if (written != data.size()) {
        emit errorOccurred(tr("数据发送不完整"));
        return false;
    }

    // 刷新发送缓冲区
    return m_serial->flush();
}

/**
 * @brief 设置读取超时时间
 * @param msec 超时时间（毫秒）
 */
void SerialManager::setReadTimeout(int msec)
{
    m_readTimeout = msec;
}

/**
 * @brief 获取当前串口名称
 * @return 串口名称
 */
QString SerialManager::portName() const
{
    return m_serial->portName();
}

/**
 * @brief 获取设备类型
 * @return 设备类型
 */
SerialManager::DeviceType SerialManager::deviceType() const
{
    return m_deviceType;
}

/**
 * @brief 串口数据可读事件处理
 *
 * 根据设备类型处理接收到的数据：
 * - 激光传感器/运动轴：按行解析，提取完整的一行数据后发送信号
 * - 液态镜头：直接透传，收到数据立即发送信号
 */
void SerialManager::onReadyRead()
{
    // 读取所有可用数据
    QByteArray data = m_serial->readAll();

    // 将数据添加到缓冲区
    m_buffer.append(data);

    // 激光传感器和运动轴：按行解析
    // 查找行结束符（\n 或 \r）
    if (m_deviceType == LaserSensor || m_deviceType == MotionAxis) {
        // 循环处理缓冲区中的所有完整行
        while (m_buffer.contains('\n') || m_buffer.contains('\r')) {
            // 找到行结束符的位置
            int idx = qMin(m_buffer.indexOf('\n'), m_buffer.indexOf('\r'));
            if (idx < 0) idx = qMax(m_buffer.indexOf('\n'), m_buffer.indexOf('\r'));
            if (idx >= 0) {
                // 提取一行数据并去除首尾空白
                QByteArray line = m_buffer.left(idx).trimmed();
                // 从缓冲区中移除已处理的数据
                m_buffer.remove(0, idx + 1);
                // 发送非空行
                if (!line.isEmpty()) {
                    emit dataReceived(line);
                }
            }
        }
    } else {
        // 液态镜头：直接透传
        if (!m_buffer.isEmpty()) {
            emit dataReceived(m_buffer);
            m_buffer.clear();
        }
    }
}

/**
 * @brief 串口错误事件处理
 * @param error 错误类型
 *
 * 当发生错误时，发送错误信号通知上层
 */
void SerialManager::onError(QSerialPort::SerialPortError error)
{
    // 忽略"NoError"这种正常状态
    if (error != QSerialPort::NoError) {
        emit errorOccurred(m_serial->errorString());
    }
}
