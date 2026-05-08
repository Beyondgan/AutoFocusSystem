/**
 * @file mainwindow.h
 * @brief 主窗口头文件
 *
 * 定义主窗口类MainWindow，负责整个应用程序的用户界面管理。
 * 包含以下功能模块：
 *   - 设备连接管理（激光传感器、液态镜头、FMC4030运动轴）
 *   - 实时数据显示（距离、焦距值、轴位置）
 *   - 轴运动控制（移动、回零、停止）
 *   - 液态镜头控制（焦距调节、标定）
 *   - 工作流控制（单点测量、连续扫描）
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "serialmanager.h"
#include "axiscontroller.h"
#include "liquidlenscontroller.h"
#include "workflowengine.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief 主窗口类
 *
 * 继承自QMainWindow，是应用程序的主界面窗口。
 * 负责协调各个硬件模块和用户交互，管理整个控制系统的工作流程。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为nullptr
     */
    MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainWindow();

private slots:
    /**
     * @brief 刷新串口列表
     */
    void onRefreshPorts();

    // ============ 设备连接相关槽函数 ============

    /**
     * @brief 连接激光传感器
     */
    void onConnectLaser();

    /**
     * @brief 断开激光传感器
     */
    void onDisconnectLaser();

    /**
     * @brief 连接液态镜头
     */
    void onConnectLens();

    /**
     * @brief 断开液态镜头
     */
    void onDisconnectLens();

    /**
     * @brief 连接FMC4030运动轴
     */
    void onConnectAxis();

    /**
     * @brief 断开FMC4030运动轴
     */
    void onDisconnectAxis();

    // ============ 轴控制相关槽函数 ============

    /**
     * @brief 轴移动到指定位置
     */
    void onAxisMoveTo();

    /**
     * @brief 轴回零
     */
    void onAxisHome();

    /**
     * @brief 轴停止运动
     */
    void onAxisStop();

    /**
     * @brief 轴速度改变
     * @param value 速度值
     */
    void onAxisSpeedChanged(int value);

    // ============ 镜头控制相关槽函数 ============

    /**
     * @brief 焦距值改变
     * @param value 焦距值
     */
    void onLensFocusChanged(int value);

    /**
     * @brief 添加标定点
     */
    void onLensCalibrate();

    /**
     * @brief 偏移量改变
     * @param value 新的偏移量值
     */
    void onLensOffsetChanged(double value);

    // ============ 工作流相关槽函数 ============

    /**
     * @brief 单点测量
     */
    void onSingleMeasure();

    /**
     * @brief 开始连续扫描
     */
    void onStartScan();

    /**
     * @brief 暂停扫描
     */
    void onPauseScan();

    /**
     * @brief 继续扫描
     */
    void onResumeScan();

    /**
     * @brief 停止扫描
     */
    void onStopScan();

    // ============ 工作流状态和信号处理 ============

    /**
     * @brief 工作流状态改变
     * @param state 新的工作流状态
     */
    void onWorkflowStateChanged(WorkflowEngine::WorkflowState state);

    /**
     * @brief 距离测量完成
     * @param distance 测量到的距离值（毫米）
     * @param focusValue 对应的焦距值
     */
    void onDistanceMeasured(double distance, int focusValue);

    /**
     * @brief 扫描点完成
     * @param point 扫描点数据
     */
    void onScanPointCompleted(const WorkflowEngine::ScanPoint &point);

    /**
     * @brief 扫描完成
     */
    void onScanFinished();

    /**
     * @brief 进度改变
     * @param current 当前进度
     * @param total 总进度
     */
    void onProgressChanged(int current, int total);

    /**
     * @brief 日志消息
     * @param msg 日志内容
     */
    void onLogMessage(const QString &msg);

    /**
     * @brief 激光传感器数据接收
     * @param data 接收到的数据
     */
    void onLaserDataReceived(const QByteArray &data);

    /**
     * @brief 轴状态更新
     * @param x X轴当前位置
     * @param y Y轴当前位置
     * @param z Z轴当前位置
     */
    void onAxisStatusUpdated(float x, float y, float z);

private:
    /**
     * @brief 初始化用户界面
     */
    void initUi();

    /**
     * @brief 初始化信号槽连接
     */
    void initConnections();

    /**
     * @brief 更新串口下拉列表
     */
    void updatePortComboBoxes();

    /**
     * @brief 更新设备连接状态显示
     */
    void updateConnectionStatus();

    /**
     * @brief 添加日志信息
     * @param msg 日志消息内容
     */
    void appendLog(const QString &msg);

    /** @brief UI界面指针 */
    Ui::MainWindow *ui;

    /** @brief 激光传感器串口管理器 */
    SerialManager *m_laserSensor;

    /** @brief FMC4030运动轴控制器 */
    AxisController *m_axisController;

    /** @brief 液态镜头控制器 */
    LiquidLensController *m_lensController;

    /** @brief 工作流引擎 */
    WorkflowEngine *m_workflow;
};

#endif
