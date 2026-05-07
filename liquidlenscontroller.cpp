#include "liquidlenscontroller.h"
#include <QDebug>
#include <QtMath>

LiquidLensController::LiquidLensController(QObject *parent)
    : QObject(parent)
    , m_currentFocus(0)
    , m_currentDistance(0.0)
    , m_minFocus(0)
    , m_maxFocus(1023)
{
    m_serial = new SerialManager(SerialManager::LiquidLens, this);
    connect(m_serial, &SerialManager::dataReceived, this, &LiquidLensController::onLensDataReceived);
    connect(m_serial, &SerialManager::errorOccurred, this, &LiquidLensController::onLensError);
}

LiquidLensController::~LiquidLensController()
{
    disconnectLens();
}

bool LiquidLensController::connectLens(const QString &portName, qint32 baudRate)
{
    return m_serial->open(portName, baudRate);
}

void LiquidLensController::disconnectLens()
{
    m_serial->close();
}

bool LiquidLensController::isConnected() const
{
    return m_serial->isOpen();
}

void LiquidLensController::setFocusByDistance(double distanceMm)
{
    m_currentDistance = distanceMm;
    int focus = distanceToFocus(distanceMm);
    setFocusDirect(focus);
    emit distanceUpdated(distanceMm);
}

void LiquidLensController::setFocusDirect(int focusValue)
{
    focusValue = qBound(m_minFocus, focusValue, m_maxFocus);
    m_currentFocus = focusValue;

    QByteArray cmd = buildLensCommand(focusValue);
    if (m_serial->sendData(cmd)) {
        emit focusChanged(focusValue);
    }
}

int LiquidLensController::currentFocusValue() const
{
    return m_currentFocus;
}

double LiquidLensController::currentDistance() const
{
    return m_currentDistance;
}

void LiquidLensController::addCalibrationPoint(double distanceMm, int focusValue)
{
    m_calibration[distanceMm] = focusValue;
}

void LiquidLensController::clearCalibrationPoints()
{
    m_calibration.clear();
}

void LiquidLensController::loadCalibration(const QMap<double, int> &points)
{
    m_calibration = points;
}

void LiquidLensController::setMinFocus(int min)
{
    m_minFocus = min;
}

void LiquidLensController::setMaxFocus(int max)
{
    m_maxFocus = max;
}

int LiquidLensController::minFocus() const
{
    return m_minFocus;
}

int LiquidLensController::maxFocus() const
{
    return m_maxFocus;
}

int LiquidLensController::distanceToFocus(double distanceMm) const
{
    if (m_calibration.isEmpty()) {
        return interpolateFocus(distanceMm);
    }
    return interpolateFocus(distanceMm);
}

int LiquidLensController::interpolateFocus(double distanceMm) const
{
    if (m_calibration.isEmpty()) {
        double ratio = qBound(0.0, (distanceMm - 50.0) / 950.0, 1.0);
        return static_cast<int>(m_minFocus + ratio * (m_maxFocus - m_minFocus));
    }

    if (m_calibration.size() == 1) {
        return m_calibration.first();
    }

    auto upper = m_calibration.lowerBound(distanceMm);

    if (upper == m_calibration.end()) {
        return m_calibration.last();
    }
    if (upper == m_calibration.begin()) {
        return m_calibration.first();
    }

    auto lower = upper - 1;
    double dLower = lower.key();
    double dUpper = upper.key();
    int fLower = lower.value();
    int fUpper = upper.value();

    if (qFuzzyCompare(dLower, dUpper)) {
        return fLower;
    }

    double ratio = (distanceMm - dLower) / (dUpper - dLower);
    return static_cast<int>(fLower + ratio * (fUpper - fLower));
}

void LiquidLensController::onLensDataReceived(const QByteArray &data)
{
    Q_UNUSED(data)
}

void LiquidLensController::onLensError(const QString &error)
{
    emit errorOccurred(error);
}

QByteArray LiquidLensController::buildLensCommand(int focusValue)
{
    return QString("FOCUS %1\r\n").arg(focusValue).toUtf8();
}
