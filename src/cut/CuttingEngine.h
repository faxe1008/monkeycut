#pragma once

#include <QObject>

#include "core/CutPlanner.h"
#include "core/Fps.h"

class QThread;

// Stream-copy (no re-encode) cutter: concatenates the planned keep-segments
// into a new file with the same container format as the input.
class CuttingEngine : public QObject
{
    Q_OBJECT
public:
    explicit CuttingEngine(QObject* parent = nullptr);
    ~CuttingEngine() override;

    bool start(const QString& inputPath, const QString& outputPath,
               const QVector<PlannedSegment>& segments, const Fps& fps);

    bool isRunning() const;

signals:
    void progress(qint64 framesDone, qint64 framesTotal);
    void finished(bool ok, const QString& message);

private:
    QThread* m_worker = nullptr;
};