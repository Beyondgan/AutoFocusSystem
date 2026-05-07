#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QTimer>

class SerialManager : public QObject
{
    Q_OBJECT

public:
    enum DeviceType {
        LaserSensor,
        LiquidLens,
        MotionAxis
    };

    explicit SerialManager(DeviceType type, QObject *parent = nullptr);
    ~SerialManager();

    static QStringList availablePorts();

    bool open(const QString &portName, qint32 baudRate = 115200,
              QSerialPort::DataBits dataBits = QSerialPort::Data8,
              QSerialPort::Parity parity = QSerialPort::NoParity,
              QSerialPort::StopBits stopBits = QSerialPort::OneStop);
    void close();
    bool isOpen() const;

    bool sendData(const QByteArray &data);
    void setReadTimeout(int msec);

    QString portName() const;
    DeviceType deviceType() const;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void connectionChanged(bool connected);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serial;
    DeviceType m_deviceType;
    QByteArray m_buffer;
    int m_readTimeout;
};

#endif // SERIALMANAGER_H
