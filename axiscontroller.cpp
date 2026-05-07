/**
 * @file axiscontroller.cpp
 * @brief FMC4030运动轴控制器实现文件
 *
 * 封装FMC4030 SDK，提供简洁的运动控制接口。
 * 主要功能包括：
 *   - 网络连接管理
 *   - 单轴运动控制
 *   - 插补运动
 *   - 位置状态轮询
 */

#include "axiscontroller.h"
#include <QDebug>
#include <windows.h>

// 引入FMC4030 DLL函数声明
#ifdef _WIN64
#define FMC4030_LIB_PATH "FMC4030\\lib\\FMC4030-Dll.lib"
#else
#define FMC4030_LIB_PATH "FMC4030\\lib\\FMC4030-Dll.lib"
#endif

#pragma comment(lib, FMC4030_LIB_PATH)

extern "C" {
    // 设备连接
    int __declspec(dllimport) __stdcall FMC4030_Open_Device(int id, char* ip, int port);
    int __declspec(dllimport) __stdcall FMC4030_Close_Device(int id);

    // 单轴运动
    int __declspec(dllimport) __stdcall FMC4030_Jog_Single_Axis(int id, int axis, float pos, float speed, float acc, float dec, int mode);
    int __declspec(dllimport) __stdcall FMC4030_Check_Axis_Is_Stop(int id, int axis);
    int __declspec(dllimport) __stdcall FMC4030_Home_Single_Axis(int id, int axis, float homeSpeed, float homeAccDec, float homeFallStep, int homeDir);
    int __declspec(dllimport) __stdcall FMC4030_Stop_Single_Axis(int id, int axis, int mode);

    // 位置查询
    int __declspec(dllimport) __stdcall FMC4030_Get_Axis_Current_Pos(int id, int axis, float* pos);
    int __declspec(dllimport) __stdcall FMC4030_Get_Axis_Current_Speed(int id, int axis, float* speed);

    // 状态查询
    int __declspec(dllimport) __stdcall FMC4030_Get_Machine_Status(int id, unsigned char* machineData);

    // 插补运动
    int __declspec(dllimport) __stdcall FMC4030_Line_2Axis(int id, unsigned int axis, float endX, float endY, float speed, float acc, float dec);
    int __declspec(dllimport) __stdcall FMC4030_Line_3Axis(int id, unsigned int axis, float endX, float endY, float endZ, float speed, float acc, float dec);

    // 运动控制
    int __declspec(dllimport) __stdcall FMC4030_Pause_Run(int id, unsigned int axis);
    int __declspec(dllimport) __stdcall FMC4030_Resume_Run(int id, unsigned int axis);
    int __declspec(dllimport) __stdcall FMC4030_Stop_Run(int id);
}

/**
 * @brief 机器状态数据结构
 *
 * 存储FMC4030返回的完整状态信息
 */
struct MachineStatus {
    float realPos[3];              // 三轴实际位置
    float realSpeed[3];            // 三轴实际速度
    unsigned int inputStatus;     // 输入端口状态
    unsigned int outputStatus;     // 输出端口状态
    unsigned int limitNStatus;     // 负限位状态
    unsigned int limitPStatus;     // 正限位状态
    unsigned int machineRunStatus; // 机器运行状态
    unsigned int axisStatus[3];    // 各轴状态
    unsigned int homeStatus;       // 回零状态
    char file[20][30];            // 文件名列表
};

/**
 * @brief 构造函数
 * @param parent 父对象指针
 *
 * 初始化成员变量和轮询定时器
 */
AxisController::AxisController(QObject *parent)
    : QObject(parent)
    , m_state(Disconnected)
    , m_speed(10.0f)
    , m_acceleration(100.0f)
    , m_connected(false)
{
    // 初始化三轴位置和目标位置为0
    for (int i = 0; i < 3; i++) {
        m_currentPos[i] = 0.0f;
        m_targetPos[i] = 0.0f;
        m_movePending[i] = false;
    }

    // 创建位置轮询定时器，间隔100ms
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, &AxisController::onPollTimeout);
}

/**
 * @brief 析构函数
 */
AxisController::~AxisController()
{
    disconnectAxis();
}

/**
 * @brief 连接到FMC4030控制器
 * @param ip IP地址
 * @param port 端口号
 * @return 连接是否成功
 */
bool AxisController::connectAxis(const QString &ip, int port)
{
    QByteArray ipBytes = ip.toUtf8();
    char* ipStr = ipBytes.data();

    // 调用SDK连接函数
    int ret = FMC4030_Open_Device(FMC4030_DEVICE_ID, ipStr, port);
    if (ret == 0) {
        m_connected = true;
        m_pollTimer->start();  // 启动位置轮询
        setState(Idle);
        qDebug() << "FMC4030 connected:" << ip << port;
        return true;
    }

    m_connected = false;
    setState(Error);
    qDebug() << "FMC4030 connect failed, ret:" << ret;
    return false;
}

/**
 * @brief 断开连接
 */
void AxisController::disconnectAxis()
{
    // 停止轮询
    m_pollTimer->stop();

    if (m_connected) {
        FMC4030_Close_Device(FMC4030_DEVICE_ID);
        m_connected = false;
    }
    setState(Disconnected);
}

/**
 * @brief 单轴绝对运动
 * @param position 目标位置（毫米）
 * @param axis 轴序号（0=X, 1=Y, 2=Z）
 *
 * 将指定轴移动到绝对位置
 */
void AxisController::moveToPosition(double position, int axis)
{
    if (!m_connected) return;

    // 将轴序号转换为位掩码 (0=X, 1=Y, 2=Z) -> (1=X, 2=Y, 4=Z)
    int axisBit = 1 << axis;

    // 调用SDK运动函数
    FMC4030_Jog_Single_Axis(FMC4030_DEVICE_ID, axisBit, (float)position, m_speed, m_acceleration, m_acceleration, 0);

    m_targetPos[axis] = (float)position;
    m_movePending[axis] = true;
    setState(Moving);
    qDebug() << "Move axis" << axis << "to" << position;
}

/**
 * @brief 单轴相对运动
 * @param distance 移动距离（毫米）
 * @param axis 轴序号
 */
void AxisController::moveRelative(double distance, int axis)
{
    if (!m_connected) return;

    // 先获取当前位置
    float currentPos = 0.0f;
    FMC4030_Get_Axis_Current_Pos(FMC4030_DEVICE_ID, 1 << axis, &currentPos);

    // 计算目标位置并移动
    moveToPosition(currentPos + distance, axis);
}

/**
 * @brief 单轴回零
 * @param axis 轴序号
 *
 * 执行原点回归操作
 */
void AxisController::home(int axis)
{
    if (!m_connected) return;

    int axisBit = 1 << axis;

    // 回零参数：速度5mm/s，加减速50，回落步距1mm，方向0
    FMC4030_Home_Single_Axis(FMC4030_DEVICE_ID, axisBit, 5.0f, 50.0f, 1.0f, 0);
    m_movePending[axis] = true;
    setState(Homing);
    qDebug() << "Home axis" << axis;
}

/**
 * @brief 单轴停止
 * @param axis 轴序号
 */
void AxisController::stop(int axis)
{
    if (!m_connected) return;

    int axisBit = 1 << axis;
    FMC4030_Stop_Single_Axis(FMC4030_DEVICE_ID, axisBit, 0);
    m_movePending[axis] = false;
    setState(Idle);
    qDebug() << "Stop axis" << axis;
}

/**
 * @brief 设置运动速度
 * @param speed 速度（毫米/秒）
 */
void AxisController::setSpeed(double speed)
{
    m_speed = (float)speed;
}

/**
 * @brief 设置加速度
 * @param accel 加速度值
 */
void AxisController::setAcceleration(double accel)
{
    m_acceleration = (float)accel;
}

/**
 * @brief 获取当前轴位置
 * @param axis 轴序号
 * @return 当前位置
 */
double AxisController::currentPosition(int axis) const
{
    if (!m_connected || axis < 0 || axis >= 3) return 0.0;
    return m_currentPos[axis];
}

/**
 * @brief 获取目标位置
 * @param axis 轴序号
 * @return 目标位置
 */
double AxisController::targetPosition(int axis) const
{
    if (axis < 0 || axis >= 3) return 0.0;
    return m_targetPos[axis];
}

/**
 * @brief 获取轴状态
 * @return 当前状态
 */
AxisController::AxisState AxisController::state() const
{
    return m_state;
}

/**
 * @brief 检查是否已连接
 * @return 连接状态
 */
bool AxisController::isConnected() const
{
    return m_connected;
}

/**
 * @brief 设置轮询间隔
 * @param msec 间隔（毫秒）
 */
void AxisController::setPollInterval(int msec)
{
    m_pollTimer->setInterval(msec);
}

/**
 * @brief 双轴直线插补
 * @param x X轴目标位置
 * @param y Y轴目标位置
 * @param speed 运动速度
 * @param acc 加速度
 * @param dec 减速度
 */
void AxisController::line2Axis(float x, float y, float speed, float acc, float dec)
{
    if (!m_connected) return;

    unsigned int axisMask = 0x03;  // X和Y轴
    FMC4030_Line_2Axis(FMC4030_DEVICE_ID, axisMask, x, y, speed, acc, dec);
    m_targetPos[0] = x;
    m_targetPos[1] = y;
    for (int i = 0; i < 2; i++) m_movePending[i] = true;
    setState(Moving);
}

/**
 * @brief 三轴直线插补
 * @param x X轴目标位置
 * @param y Y轴目标位置
 * @param z Z轴目标位置
 * @param speed 运动速度
 * @param acc 加速度
 * @param dec 减速度
 */
void AxisController::line3Axis(float x, float y, float z, float speed, float acc, float dec)
{
    if (!m_connected) return;

    unsigned int axisMask = 0x07;  // X、Y和Z轴
    FMC4030_Line_3Axis(FMC4030_DEVICE_ID, axisMask, x, y, z, speed, acc, dec);
    m_targetPos[0] = x;
    m_targetPos[1] = y;
    m_targetPos[2] = z;
    for (int i = 0; i < 3; i++) m_movePending[i] = true;
    setState(Moving);
}

/**
 * @brief 暂停运动
 */
void AxisController::pause()
{
    if (!m_connected) return;
    FMC4030_Pause_Run(FMC4030_DEVICE_ID, 0x07);
    setState(Idle);
}

/**
 * @brief 恢复运动
 */
void AxisController::resume()
{
    if (!m_connected) return;
    FMC4030_Resume_Run(FMC4030_DEVICE_ID, 0x07);
    setState(Moving);
}

/**
 * @brief 停止所有轴
 */
void AxisController::stopAll()
{
    if (!m_connected) return;
    FMC4030_Stop_Run(FMC4030_DEVICE_ID);
    for (int i = 0; i < 3; i++) m_movePending[i] = false;
    setState(Idle);
}

/**
 * @brief 轮询定时器处理
 *
 * 定期查询FMC4030状态，更新位置信息
 */
void AxisController::onPollTimeout()
{
    if (!m_connected) return;

    MachineStatus status;
    int ret = FMC4030_Get_Machine_Status(FMC4030_DEVICE_ID, (unsigned char*)&status);
    if (ret != 0) return;

    // 更新位置信息
    for (int i = 0; i < 3; i++) {
        if (qAbs(status.realPos[i] - m_currentPos[i]) > 0.001f) {
            m_currentPos[i] = status.realPos[i];
            emit positionChanged(m_currentPos[i], i);
        }
    }

    // 发送三轴位置更新信号
    emit statusUpdated(status.realPos[0], status.realPos[1], status.realPos[2]);

    // 检查各轴运动状态
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

    // 如果所有轴都停止，更新状态
    if (m_state == Moving || m_state == Homing) {
        if (!anyMoving) {
            setState(Idle);
        }
    }
}

/**
 * @brief 设置状态
 * @param state 新状态
 */
void AxisController::setState(AxisState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

/**
 * @brief 检查轴是否停止
 * @param axis 轴序号
 * @return 1=停止，0=运动中
 */
int AxisController::checkAxisStop(int axis)
{
    return FMC4030_Check_Axis_Is_Stop(FMC4030_DEVICE_ID, 1 << axis);
}
