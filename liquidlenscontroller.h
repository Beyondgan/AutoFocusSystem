/**
 * @file liquidlenscontroller.h
 * @brief 液态镜头控制器头文件
 *
 * 负责控制液态镜头的焦距调节，支持：
 *   - 直接设置焦距值
 *   - 根据距离自动计算焦距（基于标定曲线）
 *   - 标定曲线管理
 */

#ifndef LIQUIDLENSCONTROLLER_H
#define LIQUIDLENSCONTROLLER_H

#include <QObject>
#include <QMap>
#include "serialmanager.h"

/**
 * @class LiquidLensController
 * @brief 液态镜头控制器类
 *
 * 通过串口通信控制液态镜头，支持根据距离自动计算焦距。
 * 内部维护标定曲线，用于距离到焦距值的转换。
 */
class LiquidLensController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit LiquidLensController(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~LiquidLensController();

    /**
     * @brief 连接到液态镜头
     * @param portName 串口名称
     * @param baudRate 波特率
     * @return 连接是否成功
     */
    bool connectLens(const QString &portName, qint32 baudRate = 115200);

    /**
     * @brief 断开连接
     */
    void disconnectLens();

    /**
     * @brief 检查是否已连接
     * @return 连接状态
     */
    bool isConnected() const;

    /**
     * @brief 根据距离设置焦距
     * @param distanceMm 目标距离（毫米）
     *
     * 根据标定曲线自动计算并设置焦距值
     */
    void setFocusByDistance(double distanceMm);

    /**
     * @brief 直接设置焦距值
     * @param focusValue 焦距值（0-1023）
     */
    void setFocusDirect(int focusValue);

    /**
     * @brief 获取当前焦距值
     * @return 当前焦距值
     */
    int currentFocusValue() const;

    /**
     * @brief 获取当前距离
     * @return 当前距离值
     */
    double currentDistance() const;

    /**
     * @brief 添加标定点
     * @param distanceMm 距离值（毫米）
     * @param focusValue 焦距值
     */
    void addCalibrationPoint(double distanceMm, int focusValue);

    /**
     * @brief 清除所有标定点
     */
    void clearCalibrationPoints();

    /**
     * @brief 加载标定曲线
     * @param points 标定点映射（距离 -> 焦距）
     */
    void loadCalibration(const QMap<double, int> &points);

    /**
     * @brief 设置最小焦距值
     * @param min 最小值
     */
    void setMinFocus(int min);

    /**
     * @brief 设置最大焦距值
     * @param max 最大值
     */
    void setMaxFocus(int max);

    /**
     * @brief 获取最小焦距值
     * @return 最小值
     */
    int minFocus() const;

    /**
     * @brief 获取最大焦距值
     * @return 最大值
     */
    int maxFocus() const;

    /**
     * @brief 将距离转换为焦距值
     * @param distanceMm 距离值
     * @return 焦距值
     *
     * 使用标定曲线进行插值计算
     */
    int distanceToFocus(double distanceMm) const;

signals:
    /**
     * @brief 焦距改变信号
     * @param focusValue 新的焦距值
     */
    void focusChanged(int focusValue);

    /**
     * @brief 距离更新信号
     * @param distanceMm 当前距离值
     */
    void distanceUpdated(double distanceMm);

    /**
     * @brief 错误信号
     * @param error 错误描述
     */
    void errorOccurred(const QString &error);

private slots:
    /**
     * @brief 数据接收处理
     * @param data 接收到的数据
     */
    void onLensDataReceived(const QByteArray &data);

    /**
     * @brief 错误处理
     * @param error 错误描述
     */
    void onLensError(const QString &error);

private:
    /**
     * @brief 插值计算焦距
     * @param distanceMm 距离值
     * @return 焦距值
     */
    int interpolateFocus(double distanceMm) const;

    /**
     * @brief 构建镜头控制命令
     * @param focusValue 焦距值
     * @return 命令字节数组
     */
    QByteArray buildLensCommand(int focusValue);

    /** @brief 串口管理器 */
    SerialManager *m_serial;

    /** @brief 当前焦距值 */
    int m_currentFocus;

    /** @brief 当前距离值 */
    double m_currentDistance;

    /** @brief 最小焦距值 */
    int m_minFocus;

    /** @brief 最大焦距值 */
    int m_maxFocus;

    /** @brief 标定曲线（距离 -> 焦距） */
    QMap<double, int> m_calibration;
};

#endif
