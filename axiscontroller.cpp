#include "axiscontroller.h"
#include <QDebug>

AxisController::AxisController(QObject *parent)
    : QObject(parent)
    , m_state(Idle)
    , m_currentPos(0.0)
    , m_targetPos(0.0)
    , m_speed(100.0)
    , m_acceleration(500.0)
    , m_movePending(false)
{
    m_serial = new SerialManager(SerialManager::MotionAxis, this);
    connect(m_serial, &SerialManager::dataReceived, this, &AxisController::onAxisDataReceived);
    connect(m_serial, &SerialManager::errorOccurred, this, &AxisController::onAxisError);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, &AxisController::onPollTimeout);
}

AxisController::~AxisController()
{
    disconnectAxis();
}

bool AxisController::connectAxis(const QString &portName, qint32 baudRate)
{
    if (m_serial->open(portName, baudRate)) {
        m_pollTimer->start();
        setState(Idle);
        return true;
    }
    setState(Error);
    return false;
}

void AxisController::disconnectAxis()
{
    m_pollTimer->stop();
    m_serial->close();
    setState(Idle);
}

void AxisController::moveToPosition(double position)
{
    m_targetPos = position;
    m_movePending = true;
    setState(Moving);

    QByteArray cmd = buildCommand(QString("MOVE_ABS %1").arg(position, 0, 'f', 3));
    m_serial->sendData(cmd);
}

void AxisController::moveRelative(double distance)
{
    m_targetPos = m_currentPos + distance;
    m_movePending = true;
    setState(Moving);

    QByteArray cmd = buildCommand(QString("MOVE_REL %1").arg(distance, 0, 'f', 3));
    m_serial->sendData(cmd);
}

void AxisController::home()
{
    m_targetPos = 0.0;
    m_movePending = true;
    setState(Homing);

    QByteArray cmd = buildCommand("HOME");
    m_serial->sendData(cmd);
}

void AxisController::stop()
{
    QByteArray cmd = buildCommand("STOP");
    m_serial->sendData(cmd);
    m_movePending = false;
    setState(Idle);
}

void AxisController::setSpeed(double speed)
{
    m_speed = speed;
    QByteArray cmd = buildCommand(QString("SPEED %1").arg(speed, 0, 'f', 1));
    m_serial->sendData(cmd);
}

void AxisController::setAcceleration(double accel)
{
    m_acceleration = accel;
    QByteArray cmd = buildCommand(QString("ACCEL %1").arg(accel, 0, 'f', 1));
    m_serial->sendData(cmd);
}

double AxisController::currentPosition() const
{
    return m_currentPos;
}

double AxisController::targetPosition() const
{
    return m_targetPos;
}

AxisController::AxisState AxisController::state() const
{
    return m_state;
}

bool AxisController::isConnected() const
{
    return m_serial->isOpen();
}

void AxisController::setPollInterval(int msec)
{
    m_pollTimer->setInterval(msec);
}

void AxisController::onAxisDataReceived(const QByteArray &data)
{
    QString response = QString::fromUtf8(data).trimmed();

    if (response.startsWith("POS:", Qt::CaseInsensitive)) {
        bool ok = false;
        double pos = response.mid(4).trimmed().toDouble(&ok);
        if (ok) {
            m_currentPos = pos;
            emit positionChanged(m_currentPos);
        }
    } else if (response.startsWith("OK", Qt::CaseInsensitive) ||
               response.startsWith("DONE", Qt::CaseInsensitive)) {
        if (m_movePending) {
            m_movePending = false;
            setState(Idle);
            emit moveFinished(true);
        }
    } else if (response.startsWith("ERR", Qt::CaseInsensitive)) {
        m_movePending = false;
        setState(Error);
        emit moveFinished(false);
        emit errorOccurred(response);
    }
}

void AxisController::onAxisError(const QString &error)
{
    setState(Error);
    emit errorOccurred(error);
}

void AxisController::onPollTimeout()
{
    if (m_serial->isOpen()) {
        queryPosition();
    }
}

void AxisController::setState(AxisState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

void AxisController::queryPosition()
{
    QByteArray cmd = buildCommand("GET_POS");
    m_serial->sendData(cmd);
}

QByteArray AxisController::buildCommand(const QString &cmd)
{
    return (cmd + "\r\n").toUtf8();
}
