/**
 * @file axiscontroller.h
 * @brief FMC4030运动轴控制器头文件
 *
 * 通过FMC4030-Dll.dll动态链接库控制三轴运动平台。
 * 支持以下功能：
 *   - 单轴点对点运动
 *   - 双轴/三轴直线插补运动
 *   - 原点回归（回零）
 *   - 运动暂停/恢复/停止
 *   - 实时位置反馈
 */

#ifndef AXISCONTROLLER_H
#define AXISCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QHostAddress>

/** @brief FMC4030设备ID */
#define FMC4030_DEVICE_ID 0

/**
 * @class AxisController
 * @brief FMC4030运动轴控制器类
 *
 * 封装FMC4030控制器SDK，提供简洁的运动控制接口。
 * 支持X、Y、Z三个轴的独立控制和联合控制。
 */
class AxisController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 轴状态枚举
     */
    enum AxisState {
        Idle,           /**< 空闲状态 */
        Moving,         /**< 运动中 */
        Homing,         /**< 回零中 */
        Error,          /**< 错误状态 */
        Disconnected    /**< 未连接 */
    };
    Q_ENUM(AxisState)

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit AxisController(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~AxisController();

    /**
     * @brief 连接到FMC4030控制器
     * @param ip 控制器IP地址
     * @param port 端口号
     * @return 连接是否成功
     */
    bool connectAxis(const QString &ip, int port);

    /**
     * @brief 断开连接
     */
    void disconnectAxis();

    /**
     * @brief 单轴绝对运动
     * @param position 目标位置（毫米）
     * @param axis 轴序号（0=X, 1=Y, 2=Z）
     */
    void moveToPosition(double position, int axis = 0);

    /**
     * @brief 单轴相对运动
     * @param distance 移动距离（毫米）
     * @param axis 轴序号（0=X, 1=Y, 2=Z）
     */
    void moveRelative(double distance, int axis = 0);

    /**
     * @brief 单轴回零
     * @param axis 轴序号（0=X, 1=Y, 2=Z）
     */
    void home(int axis = 0);

    /**
     * @brief 单轴停止
     * @param axis 轴序号（0=X, 1=Y, 2=Z）
     */
    void stop(int axis = 0);

    /**
     * @brief 设置运动速度
     * @param speed 速度（毫米/秒）
     */
    void setSpeed(double speed);

    /**
     * @brief 设置加速度
     * @param accel 加速度值
     */
    void setAcceleration(double accel);

    /**
     * @brief 获取当前轴位置
     * @param axis 轴序号
     * @return 当前位置（毫米）
     */
    double currentPosition(int axis = 0) const;

    /**
     * @brief 获取目标位置
     * @param axis 轴序号
     * @return 目标位置（毫米）
     */
    double targetPosition(int axis = 0) const;

    /**
     * @brief 获取轴状态
     * @return 当前状态
     */
    AxisState state() const;

    /**
     * @brief 检查是否已连接
     * @return 是否已连接
     */
    bool isConnected() const;

    /**
     * @brief 设置位置轮询间隔
     * @param msec 间隔时间（毫秒）
     */
    void setPollInterval(int msec);

    /**
     * @brief 双轴直线插补运动
     * @param x X轴目标位置
     * @param y Y轴目标位置
     * @param speed 运动速度
     * @param acc 加速度
     * @param dec 减速度
     */
    void line2Axis(float x, float y, float speed, float acc, float dec);

    /**
     * @brief 三轴直线插补运动
     * @param x X轴目标位置
     * @param y Y轴目标位置
     * @param z Z轴目标位置
     * @param speed 运动速度
     * @param acc 加速度
     * @param dec 减速度
     */
    void line3Axis(float x, float y, float z, float speed, float acc, float dec);

    /**
     * @brief 暂停运动
     */
    void pause();

    /**
     * @brief 恢复运动
     */
    void resume();

    /**
     * @brief 停止所有轴运动
     */
    void stopAll();

signals:
    /**
     * @brief 轴位置改变信号
     * @param position 新位置
     * @param axis 轴序号
     */
    void positionChanged(double position, int axis);

    /**
     * @brief 运动完成信号
     * @param success 是否成功
     * @param axis 轴序号
     */
    void moveFinished(bool success, int axis);

    /**
     * @brief 状态改变信号
     * @param state 新状态
     */
    void stateChanged(AxisState state);

    /**
     * @brief 错误信号
     * @param error 错误描述
     */
    void errorOccurred(const QString &error);

    /**
     * @brief 状态更新信号（包含三轴位置）
     * @param x X轴位置
     * @param y Y轴位置
     * @param z Z轴位置
     */
    void statusUpdated(float x, float y, float z);

private slots:
    /**
     * @brief 位置轮询定时器处理
     */
    void onPollTimeout();

private:
    /**
     * @brief 设置状态
     * @param state 新状态
     */
    void setState(AxisState state);

    /**
     * @brief 检查轴是否停止
     * @param axis 轴序号
     * @return 是否停止
     */
    int checkAxisStop(int axis);

    /** @brief 当前状态 */
    AxisState m_state;

    /** @brief 三轴当前位置 */
    float m_currentPos[3];

    /** @brief 三轴目标位置 */
    float m_targetPos[3];

    /** @brief 运动速度 */
    float m_speed;

    /** @brief 加速度 */
    float m_acceleration;

    /** @brief 轮询定时器 */
    QTimer *m_pollTimer;

    /** @brief 各轴运动状态 */
    bool m_movePending[3];

    /** @brief 连接状态 */
    bool m_connected;
};

#endif
