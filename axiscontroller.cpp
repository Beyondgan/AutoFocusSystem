#include "axiscontroller.h"
#include <QDebug>
#include <windows.h>

#ifdef _WIN64
#define FMC4030_LIB_PATH "FMC4030\\lib\\FMC4030-Dll.lib"
#else
#define FMC4030_LIB_PATH "FMC4030\\lib\\FMC4030-Dll.lib"
#endif

#pragma comment(lib, FMC4030_LIB_PATH)

extern "C" {
    int __declspec(dllimport) __stdcall FMC4030_Open_Device(int id, char* ip, int port);
    int __declspec(dllimport) __stdcall FMC4030_Close_Device(int id);
    int __declspec(dllimport) __stdcall FMC4030_Jog_Single_Axis(int id, int axis, float pos, float speed, float acc, float dec, int mode);
    int __declspec(dllimport) __stdcall FMC4030_Check_Axis_Is_Stop(int id, int axis);
    int __declspec(dllimport) __stdcall FMC4030_Home_Single_Axis(int id, int axis, float homeSpeed, float homeAccDec, float homeFallStep, int homeDir);
    int __declspec(dllimport) __stdcall FMC4030_Stop_Single_Axis(int id, int axis, int mode);
    int __declspec(dllimport) __stdcall FMC4030_Get_Axis_Current_Pos(int id, int axis, float* pos);
    int __declspec(dllimport) __stdcall FMC4030_Get_Axis_Current_Speed(int id, int axis, float* speed);
    int __declspec(dllimport) __stdcall FMC4030_Get_Machine_Status(int id, unsigned char* machineData);
    int __declspec(dllimport) __stdcall FMC4030_Line_2Axis(int id, unsigned int axis, float endX, float endY, float speed, float acc, float dec);
    int __declspec(dllimport) __stdcall FMC4030_Line_3Axis(int id, unsigned int axis, float endX, float endY, float endZ, float speed, float acc, float dec);
    int __declspec(dllimport) __stdcall FMC4030_Pause_Run(int id, unsigned int axis);
    int __declspec(dllimport) __stdcall FMC4030_Resume_Run(int id, unsigned int axis);
    int __declspec(dllimport) __stdcall FMC4030_Stop_Run(int id);
}

struct MachineStatus {
    float realPos[3];
    float realSpeed[3];
    unsigned int inputStatus;
    unsigned int outputStatus;
    unsigned int limitNStatus;
    unsigned int limitPStatus;
    unsigned int machineRunStatus;
    unsigned int axisStatus[3];
    unsigned int homeStatus;
    char file[20][30];
};

AxisController::AxisController(QObject *parent)
    : QObject(parent)
    , m_state(Disconnected)
    , m_speed(10.0f)
    , m_acceleration(100.0f)
    , m_connected(false)
{
    for (int i = 0; i < 3; i++) {
        m_currentPos[i] = 0.0f;
        m_targetPos[i] = 0.0f;
        m_movePending[i] = false;
    }

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, &AxisController::onPollTimeout);
}

AxisController::~AxisController()
{
    disconnectAxis();
}

bool AxisController::connectAxis(const QString &ip, int port)
{
    QByteArray ipBytes = ip.toUtf8();
    char* ipStr = ipBytes.data();

    int ret = FMC4030_Open_Device(FMC4030_DEVICE_ID, ipStr, port);
    if (ret == 0) {
        m_connected = true;
        m_pollTimer->start();
        setState(Idle);
        qDebug() << "FMC4030 connected:" << ip << port;
        return true;
    }

    m_connected = false;
    setState(Error);
    qDebug() << "FMC4030 connect failed, ret:" << ret;
    return false;
}

void AxisController::disconnectAxis()
{
    m_pollTimer->stop();
    if (m_connected) {
        FMC4030_Close_Device(FMC4030_DEVICE_ID);
        m_connected = false;
    }
    setState(Disconnected);
}

void AxisController::moveToPosition(double position, int axis)
{
    if (!m_connected) return;

    int axisBit = 1 << axis;
    FMC4030_Jog_Single_Axis(FMC4030_DEVICE_ID, axisBit, (float)position, m_speed, m_acceleration, m_acceleration, 0);
    m_targetPos[axis] = (float)position;
    m_movePending[axis] = true;
    setState(Moving);
    qDebug() << "Move axis" << axis << "to" << position;
}

void AxisController::moveRelative(double distance, int axis)
{
    if (!m_connected) return;

    float currentPos = 0.0f;
    FMC4030_Get_Axis_Current_Pos(FMC4030_DEVICE_ID, 1 << axis, &currentPos);
    moveToPosition(currentPos + distance, axis);
}

void AxisController::home(int axis)
{
    if (!m_connected) return;

    int axisBit = 1 << axis;
    FMC4030_Home_Single_Axis(FMC4030_DEVICE_ID, axisBit, 5.0f, 50.0f, 1.0f, 0);
    m_movePending[axis] = true;
    setState(Homing);
    qDebug() << "Home axis" << axis;
}

void AxisController::stop(int axis)
{
    if (!m_connected) return;

    int axisBit = 1 << axis;
    FMC4030_Stop_Single_Axis(FMC4030_DEVICE_ID, axisBit, 0);
    m_movePending[axis] = false;
    setState(Idle);
    qDebug() << "Stop axis" << axis;
}

void AxisController::setSpeed(double speed)
{
    m_speed = (float)speed;
}

void AxisController::setAcceleration(double accel)
{
    m_acceleration = (float)accel;
}

double AxisController::currentPosition(int axis) const
{
    if (!m_connected || axis < 0 || axis >= 3) return 0.0;
    return m_currentPos[axis];
}

double AxisController::targetPosition(int axis) const
{
    if (axis < 0 || axis >= 3) return 0.0;
    return m_targetPos[axis];
}

AxisController::AxisState AxisController::state() const
{
    return m_state;
}

bool AxisController::isConnected() const
{
    return m_connected;
}

void AxisController::setPollInterval(int msec)
{
    m_pollTimer->setInterval(msec);
}

void AxisController::line2Axis(float x, float y, float speed, float acc, float dec)
{
    if (!m_connected) return;

    unsigned int axisMask = 0x03;
    FMC4030_Line_2Axis(FMC4030_DEVICE_ID, axisMask, x, y, speed, acc, dec);
    m_targetPos[0] = x;
    m_targetPos[1] = y;
    for (int i = 0; i < 2; i++) m_movePending[i] = true;
    setState(Moving);
}

void AxisController::line3Axis(float x, float y, float z, float speed, float acc, float dec)
{
    if (!m_connected) return;

    unsigned int axisMask = 0x07;
    FMC4030_Line_3Axis(FMC4030_DEVICE_ID, axisMask, x, y, z, speed, acc, dec);
    m_targetPos[0] = x;
    m_targetPos[1] = y;
    m_targetPos[2] = z;
    for (int i = 0; i < 3; i++) m_movePending[i] = true;
    setState(Moving);
}

void AxisController::pause()
{
    if (!m_connected) return;
    FMC4030_Pause_Run(FMC4030_DEVICE_ID, 0x07);
    setState(Idle);
}

void AxisController::resume()
{
    if (!m_connected) return;
    FMC4030_Resume_Run(FMC4030_DEVICE_ID, 0x07);
    setState(Moving);
}

void AxisController::stopAll()
{
    if (!m_connected) return;
    FMC4030_Stop_Run(FMC4030_DEVICE_ID);
    for (int i = 0; i < 3; i++) m_movePending[i] = false;
    setState(Idle);
}

void AxisController::onPollTimeout()
{
    if (!m_connected) return;

    MachineStatus status;
    int ret = FMC4030_Get_Machine_Status(FMC4030_DEVICE_ID, (unsigned char*)&status);
    if (ret != 0) return;

    for (int i = 0; i < 3; i++) {
        if (qAbs(status.realPos[i] - m_currentPos[i]) > 0.001f) {
            m_currentPos[i] = status.realPos[i];
            emit positionChanged(m_currentPos[i], i);
        }
    }

    emit statusUpdated(status.realPos[0], status.realPos[1], status.realPos[2]);

    bool anyMoving = false;
    for (int i = 0; i < 3; i++) {
        if (m_movePending[i]) {
            int stopped = FMC4030_Check_Axis_Is_Stop(FMC4030_DEVICE_ID, 1 << i);
            if (stopped == 1) {
                m_movePending[i] = false;
                emit moveFinished(true, i);
            } else {
                anyMoving = true;
            }
        }
    }

    if (m_state == Moving || m_state == Homing) {
        if (!anyMoving) {
            setState(Idle);
        }
    }
}

void AxisController::setState(AxisState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

int AxisController::checkAxisStop(int axis)
{
    return FMC4030_Check_Axis_Is_Stop(FMC4030_DEVICE_ID, 1 << axis);
}
