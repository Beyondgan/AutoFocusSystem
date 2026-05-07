/**
 * @file serialmanager.h
 * @brief 串口管理器头文件
 *
 * 提供统一的串口通信管理功能，支持多种设备类型：
 *   - 激光传感器（LaserSensor）
 *   - 液态镜头（LiquidLens）
 *   - 运动轴（MotionAxis）
 *
 * 主要功能：
 *   - 自动扫描可用串口
 *   - 打开/关闭串口连接
 *   - 发送和接收数据
 *   - 数据自动解析（按行解析或直接透传）
 */

#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

/**
 * @class SerialManager
 * @brief 串口管理器类
 *
 * 负责管理串口通信，支持多种设备类型的数据解析。
 * 对于激光传感器和运动轴，自动按行解析数据；
 * 对于液态镜头，数据直接透传不做解析。
 */
class SerialManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 设备类型枚举
     */
    enum DeviceType {
        LaserSensor,   /**< 激光传感器 - 按行解析数据 */
        LiquidLens,    /**< 液态镜头 - 直接透传 */
        MotionAxis     /**< 运动轴 - 按行解析数据 */
    };

    /**
     * @brief 构造函数
     * @param type 设备类型
     * @param parent 父对象指针
     */
    explicit SerialManager(DeviceType type, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~SerialManager();

    /**
     * @brief 获取系统中可用的串口列表
     * @return 可用串口名称列表
     */
    static QStringList availablePorts();

    /**
     * @brief 打开串口连接
     * @param portName 串口名称（如 "COM1"）
     * @param baudRate 波特率（默认115200）
     * @param dataBits 数据位（默认8位）
     * @param parity 校验位（默认无校验）
     * @param stopBits 停止位（默认1位）
     * @return 打开是否成功
     */
    bool open(const QString &portName, qint32 baudRate = 115200,
              QSerialPort::DataBits dataBits = QSerialPort::Data8,
              QSerialPort::Parity parity = QSerialPort::NoParity,
              QSerialPort::StopBits stopBits = QSerialPort::OneStop);

    /**
     * @brief 关闭串口连接
     */
    void close();

    /**
     * @brief 检查串口是否打开
     * @return 是否已打开
     */
    bool isOpen() const;

    /**
     * @brief 发送数据
     * @param data 要发送的数据
     * @return 发送是否成功
     */
    bool sendData(const QByteArray &data);

    /**
     * @brief 设置读取超时时间
     * @param msec 超时时间（毫秒）
     */
    void setReadTimeout(int msec);

    /**
     * @brief 获取当前串口名称
     * @return 串口名称
     */
    QString portName() const;

    /**
     * @brief 获取设备类型
     * @return 设备类型
     */
    DeviceType deviceType() const;

signals:
    /**
     * @brief 数据接收信号
     * @param data 接收到的数据
     */
    void dataReceived(const QByteArray &data);

    /**
     * @brief 错误信号
     * @param error 错误描述
     */
    void errorOccurred(const QString &error);

    /**
     * @brief 连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionChanged(bool connected);

private slots:
    /**
     * @brief 串口数据可读事件处理
     */
    void onReadyRead();

    /**
     * @brief 串口错误事件处理
     * @param error 错误类型
     */
    void onError(QSerialPort::SerialPortError error);

private:
    /**
     * @brief 串口对象指针
     */
    QSerialPort *m_serial;

    /**
     * @brief 设备类型
     */
    DeviceType m_deviceType;

    /**
     * @brief 数据接收缓冲区
     */
    QByteArray m_buffer;

    /**
     * @brief 读取超时时间（毫秒）
     */
    int m_readTimeout;
};

#endif
