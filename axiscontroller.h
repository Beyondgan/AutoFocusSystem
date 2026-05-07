#ifndef AXISCONTROLLER_H
#define AXISCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QHostAddress>

#define FMC4030_DEVICE_ID 0

class AxisController : public QObject
{
    Q_OBJECT

public:
    enum AxisState {
        Idle,
        Moving,
        Homing,
        Error,
        Disconnected
    };
    Q_ENUM(AxisState)

    explicit AxisController(QObject *parent = nullptr);
    ~AxisController();

    bool connectAxis(const QString &ip, int port);
    void disconnectAxis();

    void moveToPosition(double position, int axis = 0);
    void moveRelative(double distance, int axis = 0);
    void home(int axis = 0);
    void stop(int axis = 0);
    void setSpeed(double speed);
    void setAcceleration(double accel);

    double currentPosition(int axis = 0) const;
    double targetPosition(int axis = 0) const;
    AxisState state() const;
    bool isConnected() const;

    void setPollInterval(int msec);

    void line2Axis(float x, float y, float speed, float acc, float dec);
    void line3Axis(float x, float y, float z, float speed, float acc, float dec);

    void pause();
    void resume();
    void stopAll();

signals:
    void positionChanged(double position, int axis);
    void moveFinished(bool success, int axis);
    void stateChanged(AxisState state);
    void errorOccurred(const QString &error);
    void statusUpdated(float x, float y, float z);

private slots:
    void onPollTimeout();

private:
    void setState(AxisState state);
    int checkAxisStop(int axis);

    AxisState m_state;
    float m_currentPos[3];
    float m_targetPos[3];
    float m_speed;
    float m_acceleration;
    QTimer *m_pollTimer;
    bool m_movePending[3];
    bool m_connected;
};

#endif
