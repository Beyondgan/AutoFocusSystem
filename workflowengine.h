/**
 * @file workflowengine.h
 * @brief 工作流引擎头文件
 *
 * 协调激光传感器、FMC4030运动轴和液态镜头的工作流程。
 * 主要功能：
 *   - 单点测量：触发测距→计算焦距→调节镜头
 *   - 连续扫描：轴运动→等待到位→测距→调焦→下一个点
 *   - 状态管理：暂停/恢复/停止
 */

#ifndef WORKFLOWENGINE_H
#define WORKFLOWENGINE_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include "serialmanager.h"
#include "axiscontroller.h"
#include "liquidlenscontroller.h"

/**
 * @class WorkflowEngine
 * @brief 工作流引擎类
 *
 * 管理整个自动对焦系统的工作流程，协调各硬件模块的时序。
 */
class WorkflowEngine : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 工作流状态枚举
     */
    enum WorkflowState {
        Stopped,           /**< 已停止 */
        Initializing,     /**< 初始化中 */
        WaitingForAxis,   /**< 等待轴到位 */
        Measuring,         /**< 测量中 */
        AdjustingFocus,   /**< 调焦中 */
        Paused,            /**< 已暂停 */
        Error              /**< 错误 */
    };
    Q_ENUM(WorkflowState)

    /**
     * @brief 扫描点数据结构
     */
    struct ScanPoint {
        double position;        /**< 轴位置 */
        double measuredDistance;/**< 测量距离 */
        int focusValue;        /**< 焦距值 */
    };

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit WorkflowEngine(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~WorkflowEngine();

    /**
     * @brief 设置激光传感器
     * @param sensor 传感器指针
     */
    void setLaserSensor(SerialManager *sensor);

    /**
     * @brief 设置轴控制器
     * @param axis 轴控制器指针
     */
    void setAxisController(AxisController *axis);

    /**
     * @brief 设置液态镜头控制器
     * @param lens 镜头控制器指针
     */
    void setLiquidLens(LiquidLensController *lens);

    /**
     * @brief 开始单点测量
     */
    void startSinglePoint();

    /**
     * @brief 开始连续扫描
     * @param startPos 起始位置
     * @param endPos 终止位置
     * @param stepSize 步进值
     */
    void startContinuousScan(double startPos, double endPos, double stepSize);

    /**
     * @brief 开始连续扫描（使用位置列表）
     * @param positions 位置列表
     */
    void startContinuousScan(const QList<double> &positions);

    /**
     * @brief 暂停工作流
     */
    void pause();

    /**
     * @brief 恢复工作流
     */
    void resume();

    /**
     * @brief 停止工作流
     */
    void stop();

    /**
     * @brief 获取当前状态
     * @return 当前工作流状态
     */
    WorkflowState state() const;

    /**
     * @brief 获取扫描结果
     * @return 扫描点列表
     */
    QList<ScanPoint> scanResults() const;

    /**
     * @brief 清除扫描结果
     */
    void clearResults();

    /**
     * @brief 设置测量延迟
     * @param msec 延迟时间（毫秒）
     */
    void setMeasureDelay(int msec);

    /**
     * @brief 设置是否启用自动对焦
     * @param enabled 是否启用
     */
    void setAutoFocusEnabled(bool enabled);

    /**
     * @brief 设置扫描轴
     * @param axis 轴序号（0=X, 1=Y, 2=Z）
     */
    void setScanAxis(int axis);

    /**
     * @brief 设置镜头调节间隔
     * @param intervalMs 调节间隔（毫秒），0表示全速调节
     */
    void setLensAdjustInterval(int intervalMs);

    /**
     * @brief 获取镜头调节间隔
     * @return 调节间隔（毫秒）
     */
    int lensAdjustInterval() const;

signals:
    /**
     * @brief 状态改变信号
     * @param state 新状态
     */
    void stateChanged(WorkflowState state);

    /**
     * @brief 距离测量完成信号
     * @param distance 测量距离
     * @param focusValue 焦距值
     */
    void distanceMeasured(double distance, int focusValue);

    /**
     * @brief 扫描点完成信号
     * @param point 扫描点数据
     */
    void scanPointCompleted(const ScanPoint &point);

    /**
     * @brief 扫描完成信号
     */
    void scanFinished();

    /**
     * @brief 进度改变信号
     * @param current 当前进度
     * @param total 总进度
     */
    void progressChanged(int current, int total);

    /**
     * @brief 错误信号
     * @param error 错误描述
     */
    void errorOccurred(const QString &error);

    /**
     * @brief 日志消息信号
     * @param msg 日志内容
     */
    void logMessage(const QString &msg);

private slots:
    /**
     * @brief 传感器数据接收处理
     * @param data 接收到的数据
     */
    void onSensorDataReceived(const QByteArray &data);

    /**
     * @brief 轴运动完成处理
     * @param success 是否成功
     * @param axis 轴序号
     */
    void onAxisMoveFinished(bool success, int axis);

    /**
     * @brief 轴位置改变处理
     * @param position 当前位置
     * @param axis 轴序号
     */
    void onAxisPositionChanged(double position, int axis);

    /**
     * @brief 测量超时处理
     */
    void onMeasureTimeout();

private:
    /**
     * @brief 设置工作流状态
     * @param state 新状态
     */
    void setState(WorkflowState state);

    /**
     * @brief 处理下一个扫描点
     */
    void processNextPoint();

    /**
     * @brief 解析距离值
     * @param data 原始数据
     * @return 距离值，解析失败返回-1
     */
    double parseDistance(const QByteArray &data);

    /**
     * @brief 请求测量
     */
    void requestMeasurement();

    /** @brief 激光传感器 */
    SerialManager *m_laserSensor;

    /** @brief 轴控制器 */
    AxisController *m_axis;

    /** @brief 液态镜头控制器 */
    LiquidLensController *m_lens;

    /** @brief 当前状态 */
    WorkflowState m_state;

    /** @brief 扫描位置队列 */
    QQueue<double> m_scanPositions;

    /** @brief 扫描结果列表 */
    QList<ScanPoint> m_results;

    /** @brief 当前索引 */
    int m_currentIndex;

    /** @brief 总点数 */
    int m_totalPoints;

    /** @brief 测量定时器 */
    QTimer *m_measureTimer;

    /** @brief 测量延迟 */
    int m_measureDelay;

    /** @brief 是否启用自动对焦 */
    bool m_autoFocusEnabled;

    /** @brief 是否为连续扫描模式 */
    bool m_continuousMode;

    /** @brief 上次测量距离 */
    double m_lastDistance;

    /** @brief 当前轴位置 */
    double m_currentAxisPos;

    /** @brief 扫描轴序号 */
    int m_scanAxis;

    /** @brief 待处理距离值 */
    double m_pendingDistance;

    /** @brief 镜头调节间隔（毫秒） */
    int m_lensAdjustInterval;

    /** @brief 镜头调节定时器 */
    QTimer *m_lensAdjustTimer;

    /** @brief 最近的传感器数据 */
    double m_latestSensorDistance;
};

#endif
