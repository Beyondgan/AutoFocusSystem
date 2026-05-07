#include "workflowengine.h"
#include <QDebug>
#include <QRegularExpression>

WorkflowEngine::WorkflowEngine(QObject *parent)
    : QObject(parent)
    , m_laserSensor(nullptr)
    , m_axis(nullptr)
    , m_lens(nullptr)
    , m_state(Stopped)
    , m_currentIndex(0)
    , m_totalPoints(0)
    , m_measureDelay(200)
    , m_autoFocusEnabled(true)
    , m_continuousMode(false)
    , m_lastDistance(0.0)
    , m_currentAxisPos(0.0)
    , m_scanAxis(0)
    , m_pendingDistance(0.0)
{
    m_measureTimer = new QTimer(this);
    m_measureTimer->setSingleShot(true);
    connect(m_measureTimer, &QTimer::timeout, this, &WorkflowEngine::onMeasureTimeout);
}

WorkflowEngine::~WorkflowEngine()
{
    stop();
}

void WorkflowEngine::setLaserSensor(SerialManager *sensor)
{
    if (m_laserSensor) {
        disconnect(m_laserSensor, nullptr, this, nullptr);
    }
    m_laserSensor = sensor;
    if (m_laserSensor) {
        connect(m_laserSensor, &SerialManager::dataReceived,
                this, &WorkflowEngine::onSensorDataReceived);
    }
}

void WorkflowEngine::setAxisController(AxisController *axis)
{
    if (m_axis) {
        disconnect(m_axis, nullptr, this, nullptr);
    }
    m_axis = axis;
    if (m_axis) {
        connect(m_axis, &AxisController::moveFinished,
                this, &WorkflowEngine::onAxisMoveFinished);
        connect(m_axis, &AxisController::positionChanged,
                this, &WorkflowEngine::onAxisPositionChanged);
    }
}

void WorkflowEngine::setLiquidLens(LiquidLensController *lens)
{
    m_lens = lens;
}

void WorkflowEngine::startSinglePoint()
{
    if (!m_laserSensor || !m_laserSensor->isOpen()) {
        emit errorOccurred(tr("Laser sensor not connected"));
        return;
    }

    m_continuousMode = false;
    m_scanPositions.clear();
    setState(Measuring);
    emit logMessage(tr("Single point measure started"));

    requestMeasurement();
}

void WorkflowEngine::startContinuousScan(double startPos, double endPos, double stepSize)
{
    QList<double> positions;
    if (stepSize <= 0) {
        emit errorOccurred(tr("Step size must be greater than 0"));
        return;
    }

    double pos = startPos;
    if (startPos <= endPos) {
        while (pos <= endPos + stepSize * 0.01) {
            positions.append(pos);
            pos += stepSize;
        }
    } else {
        while (pos >= endPos - stepSize * 0.01) {
            positions.append(pos);
            pos -= stepSize;
        }
    }

    startContinuousScan(positions);
}

void WorkflowEngine::startContinuousScan(const QList<double> &positions)
{
    if (!m_laserSensor || !m_laserSensor->isOpen()) {
        emit errorOccurred(tr("Laser sensor not connected"));
        return;
    }
    if (!m_axis || !m_axis->isConnected()) {
        emit errorOccurred(tr("FMC4030 not connected"));
        return;
    }
    if (positions.isEmpty()) {
        emit errorOccurred(tr("Scan position list is empty"));
        return;
    }

    m_continuousMode = true;
    m_scanPositions.clear();
    for (double pos : positions) {
        m_scanPositions.enqueue(pos);
    }
    m_totalPoints = m_scanPositions.size();
    m_currentIndex = 0;
    m_results.clear();

    setState(WaitingForAxis);
    emit logMessage(tr("Continuous scan started, total %1 points").arg(m_totalPoints));

    processNextPoint();
}

void WorkflowEngine::pause()
{
    if (m_state == WaitingForAxis || m_state == Measuring || m_state == AdjustingFocus) {
        setState(Paused);
        m_measureTimer->stop();
        if (m_axis) m_axis->pause();
        emit logMessage(tr("Workflow paused"));
    }
}

void WorkflowEngine::resume()
{
    if (m_state == Paused) {
        setState(WaitingForAxis);
        emit logMessage(tr("Workflow resumed"));
        processNextPoint();
    }
}

void WorkflowEngine::stop()
{
    m_measureTimer->stop();
    m_scanPositions.clear();
    if (m_axis) m_axis->stopAll();
    setState(Stopped);
    emit logMessage(tr("Workflow stopped"));
}

WorkflowEngine::WorkflowState WorkflowEngine::state() const
{
    return m_state;
}

QList<WorkflowEngine::ScanPoint> WorkflowEngine::scanResults() const
{
    return m_results;
}

void WorkflowEngine::clearResults()
{
    m_results.clear();
}

void WorkflowEngine::setMeasureDelay(int msec)
{
    m_measureDelay = msec;
}

void WorkflowEngine::setAutoFocusEnabled(bool enabled)
{
    m_autoFocusEnabled = enabled;
}

void WorkflowEngine::setScanAxis(int axis)
{
    m_scanAxis = axis;
}

void WorkflowEngine::onSensorDataReceived(const QByteArray &data)
{
    if (m_state != Measuring) return;

    double distance = parseDistance(data);
    if (distance < 0) return;

    m_lastDistance = distance;
    m_pendingDistance = distance;

    int focusValue = 0;
    if (m_autoFocusEnabled && m_lens && m_lens->isConnected()) {
        m_lens->setFocusByDistance(distance);
        focusValue = m_lens->currentFocusValue();
    }

    emit distanceMeasured(distance, focusValue);
    emit logMessage(tr("Distance: %1 mm, Focus: %2").arg(distance, 0, 'f', 2).arg(focusValue));

    if (m_continuousMode) {
        ScanPoint point;
        point.position = m_currentAxisPos;
        point.measuredDistance = distance;
        point.focusValue = focusValue;
        m_results.append(point);
        emit scanPointCompleted(point);

        m_currentIndex++;
        emit progressChanged(m_currentIndex, m_totalPoints);

        if (m_scanPositions.isEmpty()) {
            setState(Stopped);
            emit scanFinished();
            emit logMessage(tr("Continuous scan completed, total %1 points").arg(m_results.size()));
        } else {
            setState(WaitingForAxis);
            processNextPoint();
        }
    } else {
        ScanPoint point;
        point.position = m_currentAxisPos;
        point.measuredDistance = distance;
        point.focusValue = focusValue;
        m_results.append(point);
        emit scanPointCompleted(point);

        setState(Stopped);
        emit logMessage(tr("Single point measure completed"));
    }
}

void WorkflowEngine::onAxisMoveFinished(bool success, int axis)
{
    Q_UNUSED(axis)
    if (m_state != WaitingForAxis) return;

    if (!success) {
        setState(Error);
        emit errorOccurred(tr("Axis move failed"));
        return;
    }

    emit logMessage(tr("Axis move completed, start measuring"));
    setState(Measuring);
    requestMeasurement();
}

void WorkflowEngine::onAxisPositionChanged(double position, int axis)
{
    Q_UNUSED(axis)
    m_currentAxisPos = position;
}

void WorkflowEngine::onMeasureTimeout()
{
    if (m_state != Measuring) return;

    emit logMessage(tr("Measure timeout, retrying..."));
    requestMeasurement();
}

void WorkflowEngine::setState(WorkflowState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

void WorkflowEngine::processNextPoint()
{
    if (m_scanPositions.isEmpty()) return;

    double targetPos = m_scanPositions.dequeue();
    emit logMessage(tr("Moving to position: %1 mm").arg(targetPos, 0, 'f', 2));

    if (m_axis) {
        m_axis->moveToPosition(targetPos, m_scanAxis);
    }
}

double WorkflowEngine::parseDistance(const QByteArray &data)
{
    QString str = QString::fromUtf8(data).trimmed();

    QRegularExpression re(R"([+-]?\d+\.?\d*)");
    QRegularExpressionMatch match = re.match(str);
    if (match.hasMatch()) {
        bool ok = false;
        double val = match.captured(0).toDouble(&ok);
        if (ok && val >= 0) return val;
    }
    return -1.0;
}

void WorkflowEngine::requestMeasurement()
{
    if (m_laserSensor && m_laserSensor->isOpen()) {
        QByteArray cmd = "MEASURE\r\n";
        m_laserSensor->sendData(cmd);
        m_measureTimer->start(m_measureDelay * 5);
    }
}
