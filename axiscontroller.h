#ifndef AXISCONTROLLER_H
#define AXISCONTROLLER_H

#include <QObject>
#include <QTimer>
#include "serialmanager.h"

class AxisController : public QObject
{
    Q_OBJECT

public:
    enum AxisState {
        Idle,
        Moving,
        Homing,
        Error
    };
    Q_ENUM(AxisState)

    explicit AxisController(QObject *parent = nullptr);
    ~AxisController();

    bool connectAxis(const QString &portName, qint32 baudRate = 115200);
    void disconnectAxis();

    void moveToPosition(double position);
    void moveRelative(double distance);
    void home();
    void stop();
    void setSpeed(double speed);
    void setAcceleration(double accel);

    double currentPosition() const;
    double targetPosition() const;
    AxisState state() const;
    bool isConnected() const;

    void setPollInterval(int msec);

signals:
    void positionChanged(double position);
    void moveFinished(bool success);
    void stateChanged(AxisState state);
    void errorOccurred(const QString &error);

private slots:
    void onAxisDataReceived(const QByteArray &data);
    void onAxisError(const QString &error);
    void onPollTimeout();

private:
    void setState(AxisState state);
    void queryPosition();
    QByteArray buildCommand(const QString &cmd);

    SerialManager *m_serial;
    AxisState m_state;
    double m_currentPos;
    double m_targetPos;
    double m_speed;
    double m_acceleration;
    QTimer *m_pollTimer;
    bool m_movePending;
};

#endif // AXISCONTROLLER_H
