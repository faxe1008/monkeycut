#include "CuttingEngine.h"

#include "av/ffmpeg_c.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QThread>
#include <QString>

namespace
{

// Stream-copy cutter worker.
//
// Each keep-segment [a, b) (already keyframe-aligned on `a` by CutPlanner) is
// demuxed and copied to the output. Output timestamps are:
//   out_pts = in_pts - segStartTs + runningOffset
// so consecutive segments concatenate into one continuous timeline.
// Audio is cut at packet granularity (at most one audio frame of drift per
// cut point).
class CutWorker : public QThread
{
public:
    CutWorker(CuttingEngine* owner, QString in, QString out,
              QVector<PlannedSegment> segs, Fps fps)
        : m_owner(owner)
        , m_in(std::move(in))
        , m_out(std::move(out))
        , m_segments(std::move(segs))
        , m_fps(fps)
    {
    }

protected:
    void run() override
    {
        const bool ok = doCut();
        emit m_owner->progress(framesDone(), totalFrames());
        emit m_owner->finished(ok, ok ? QString() : m_err);
    }

    qint64 framesDone() const
    {
        return m_framesDone;
    }
    qint64 totalFrames() const
    {
        return m_totalFrames;
    }

private:
    qint64 msToTb(qint64 ms, const AVRational& tb) const
    {
        return av_rescale_q(ms, AVRational{1, 1000}, tb);
    }

    void cleanup(AVFormatContext*& in, AVFormatContext*& out)
    {
        if (in)
            avformat_close_input(&in);
        if (out) {
            if (out->pb)
                avio_closep(&out->pb);
            avformat_free_context(out);
        }
    }

    bool doCut()
    {
        AVFormatContext* in = nullptr;
        AVFormatContext* out = nullptr;
        bool ok = true;

        if (avformat_open_input(&in, m_in.toUtf8().constData(), nullptr, nullptr) < 0) {
            m_err = QStringLiteral("Cannot open input file");
            cleanup(in, out);
            return false;
        }
        if (avformat_find_stream_info(in, nullptr) < 0) {
            m_err = QStringLiteral("Cannot read input stream info");
            cleanup(in, out);
            return false;
        }

        int iV = -1, iA = -1;
        for (unsigned i = 0; i < in->nb_streams; ++i) {
            const AVMediaType t = in->streams[i]->codecpar->codec_type;
            if (t == AVMEDIA_TYPE_VIDEO && iV < 0)
                iV = int(i);
            else if (t == AVMEDIA_TYPE_AUDIO && iA < 0)
                iA = int(i);
        }
        if (iV < 0) {
            m_err = QStringLiteral("No video stream in input");
            cleanup(in, out);
            return false;
        }
        AVStream* iv = in->streams[iV];
        AVStream* ia = iA >= 0 ? in->streams[iA] : nullptr;

        if (avformat_alloc_output_context2(&out, nullptr, nullptr,
                                           m_out.toUtf8().constData())
            < 0) {
            m_err = QStringLiteral("Cannot create output for “%1”")
                        .arg(QFileInfo(m_out).fileName());
            cleanup(in, out);
            return false;
        }

        {
            AVStream* os = avformat_new_stream(out, nullptr);
            if (!os || avcodec_parameters_copy(os->codecpar, iv->codecpar) < 0
                || os->codecpar == nullptr) {
                m_err = QStringLiteral("Cannot map video stream");
                cleanup(in, out);
                return false;
            }
            os->time_base = iv->time_base;
        }
        bool haveAudio = false;
        if (ia) {
            AVStream* os = avformat_new_stream(out, nullptr);
            if (os && avcodec_parameters_copy(os->codecpar, ia->codecpar) >= 0
                && os->codecpar != nullptr) {
                os->time_base = ia->time_base;
                haveAudio = true;
            }
        }

        if (!(out->oformat->flags & AVFMT_NOFILE)
            && avio_open(&out->pb, m_out.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            m_err = QStringLiteral("Cannot open output file");
            cleanup(in, out);
            return false;
        }
        if (avformat_write_header(out, nullptr) < 0) {
            m_err = QStringLiteral("Cannot write output header");
            cleanup(in, out);
            return false;
        }

        const qint64 Rv = iv->start_time != AV_NOPTS_VALUE ? iv->start_time : 0;
        const qint64 Ra = ia ? (ia->start_time != AV_NOPTS_VALUE ? ia->start_time : 0)
                             : 0;

        qint64 offV = 0; // running output offset, video tb
        qint64 offA = 0; // running output offset, audio tb
        m_framesDone = 0;
        m_totalFrames = 0;
        for (const auto& s : m_segments)
            m_totalFrames += s.outFrame - s.inFrame;

        AVPacket* pkt = av_packet_alloc();

        for (int segIdx = 0; segIdx < m_segments.size() && ok; ++segIdx) {
            const PlannedSegment& seg = m_segments[segIdx];
            const qint64 aMs = qRound(double(seg.inFrame) * 1000.0 / m_fps.value());
            const qint64 bMs = qRound(double(seg.outFrame) * 1000.0 / m_fps.value());

            // absolute file-timeline boundaries (each stream may start at an
            // offset, e.g. TS pre-roll), expressed in the stream's time base
            const qint64 aStartV = Rv + msToTb(aMs, iv->time_base);
            const qint64 aEndV = Rv + msToTb(bMs, iv->time_base);
            const qint64 aStartA = ia ? (Ra + msToTb(aMs, ia->time_base)) : 0;
            const qint64 aEndA = ia ? (Ra + msToTb(bMs, ia->time_base)) : 0;

            qint64 seekTs = aStartV - 1;
            if (seekTs < 0)
                seekTs = 0;
            if (av_seek_frame(in, iV, seekTs, AVSEEK_FLAG_BACKWARD) < 0) {
                m_err = QStringLiteral("Seek failed at segment %1").arg(segIdx + 1);
                ok = false;
                break;
            }

            bool vPastEnd = false;
            bool readEof = false;
            for (;;) {
                const int r = av_read_frame(in, pkt);
                if (r < 0) {
                    readEof = true;
                    break;
                }

                if (pkt->stream_index == iV) {
                    const bool hasPts = pkt->pts != AV_NOPTS_VALUE;
                    if (hasPts && pkt->pts >= aEndV) {
                        av_packet_unref(pkt); // first video packet past the end
                        vPastEnd = true;
                        break;
                    }
                    if (hasPts && pkt->pts < aStartV) {
                        av_packet_unref(pkt); // before segment start
                        continue;
                    }
                    pkt->pts = (hasPts ? pkt->pts : pkt->dts) - aStartV + offV;
                    if (pkt->dts != AV_NOPTS_VALUE)
                        pkt->dts = pkt->dts - aStartV + offV;
                    pkt->pos = -1;
                    if (av_interleaved_write_frame(out, pkt) < 0) {
                        m_err =
                            QStringLiteral("Write failed at segment %1").arg(segIdx + 1);
                        av_packet_unref(pkt);
                        ok = false;
                        break;
                    }
                    av_packet_unref(pkt);
                    ++m_framesDone;
                    if (m_framesDone % 25 == 0)
                        emit m_owner->progress(m_framesDone, m_totalFrames);
                } else if (ia && pkt->stream_index == iA) {
                    const bool hasPts = pkt->pts != AV_NOPTS_VALUE;
                    if (hasPts && (pkt->pts >= aEndA || pkt->pts < aStartA)) {
                        av_packet_unref(pkt); // outside the segment
                        continue;
                    }
                    pkt->pts = (hasPts ? pkt->pts : pkt->dts) - aStartA + offA;
                    if (pkt->dts != AV_NOPTS_VALUE)
                        pkt->dts = pkt->dts - aStartA + offA;
                    pkt->pos = -1;
                    if (av_interleaved_write_frame(out, pkt) < 0) {
                        m_err =
                            QStringLiteral("Write failed at segment %1").arg(segIdx + 1);
                        av_packet_unref(pkt);
                        ok = false;
                        break;
                    }
                    av_packet_unref(pkt);
                } else {
                    av_packet_unref(pkt);
                }
            }

            if (!ok)
                break;
            if (readEof) {
                if (!vPastEnd)
                    m_err = QStringLiteral("Unexpected end of input at segment %1")
                                .arg(segIdx + 1);
                break; // nothing more to read
            }
            // advance running offsets by the segment length
            offV += aEndV - aStartV;
            if (ia)
                offA += aEndA - aStartA;
        }

        av_packet_free(&pkt);

        if (ok && out && av_write_trailer(out) < 0) {
            m_err = QStringLiteral("Failed to write output trailer");
            ok = false;
        }

        cleanup(in, out);
        return ok;
    }

    CuttingEngine* m_owner;
    QString m_in;
    QString m_out;
    QVector<PlannedSegment> m_segments;
    Fps m_fps;
    QString m_err;
    qint64 m_framesDone = 0;
    qint64 m_totalFrames = 0;
};
}

CuttingEngine::CuttingEngine(QObject* parent)
    : QObject(parent)
{
}

CuttingEngine::~CuttingEngine()
{
    if (m_worker) {
        m_worker->wait(3000);
        delete m_worker;
    }
}

bool CuttingEngine::start(const QString& inputPath, const QString& outputPath,
                          const QVector<PlannedSegment>& segments, const Fps& fps)
{
    if (segments.isEmpty() || !fps.isValid()) {
        emit finished(false, QStringLiteral("Nothing to cut"));
        return false;
    }
    if (m_worker) {
        emit finished(false, QStringLiteral("An export is already running"));
        return false;
    }
    m_worker = new CutWorker(this, inputPath, outputPath, segments, fps);
    m_worker->start();
    return true;
}

bool CuttingEngine::isRunning() const
{
    return m_worker != nullptr;
}