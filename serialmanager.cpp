#include "serialmanager.h"
#include <QDebug>

SerialManager::SerialManager(DeviceType type, QObject *parent)
    : QObject(parent)
    , m_deviceType(type)
    , m_readTimeout(100)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialManager::onError);
}

SerialManager::~SerialManager()
{
    close();
}

QStringList SerialManager::availablePorts()
{
    QStringList list;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        list << info.portName();
    }
    return list;
}

bool SerialManager::open(const QString &portName, qint32 baudRate,
                          QSerialPort::DataBits dataBits,
                          QSerialPort::Parity parity,
                          QSerialPort::StopBits stopBits)
{
    close();
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(dataBits);
    m_serial->setParity(parity);
    m_serial->setStopBits(stopBits);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        m_buffer.clear();
        emit connectionChanged(true);
        return true;
    }

    emit errorOccurred(tr("无法打开串口 %1: %2").arg(portName, m_serial->errorString()));
    return false;
}

void SerialManager::close()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);
    }
}

bool SerialManager::isOpen() const
{
    return m_serial->isOpen();
}

bool SerialManager::sendData(const QByteArray &data)
{
    if (!m_serial->isOpen()) {
        emit errorOccurred(tr("串口未打开，无法发送数据"));
        return false;
    }
    qint64 written = m_serial->write(data);
    if (written != data.size()) {
        emit errorOccurred(tr("数据发送不完整"));
        return false;
    }
    return m_serial->flush();
}

void SerialManager::setReadTimeout(int msec)
{
    m_readTimeout = msec;
}

QString SerialManager::portName() const
{
    return m_serial->portName();
}

SerialManager::DeviceType SerialManager::deviceType() const
{
    return m_deviceType;
}

void SerialManager::onReadyRead()
{
    QByteArray data = m_serial->readAll();
    m_buffer.append(data);

    if (m_deviceType == LaserSensor || m_deviceType == MotionAxis) {
        while (m_buffer.contains('\n') || m_buffer.contains('\r')) {
            int idx = qMin(m_buffer.indexOf('\n'), m_buffer.indexOf('\r'));
            if (idx < 0) idx = qMax(m_buffer.indexOf('\n'), m_buffer.indexOf('\r'));
            if (idx >= 0) {
                QByteArray line = m_buffer.left(idx).trimmed();
                m_buffer.remove(0, idx + 1);
                if (!line.isEmpty()) {
                    emit dataReceived(line);
                }
            }
        }
    } else {
        if (!m_buffer.isEmpty()) {
            emit dataReceived(m_buffer);
            m_buffer.clear();
        }
    }
}

void SerialManager::onError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        emit errorOccurred(m_serial->errorString());
    }
}
