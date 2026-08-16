#include "AvProbe.h"
#include "VfrDetect.h"
#include "ffmpeg_c.h"

#include <QFileInfo>
#include <QtGlobal>

static Fps videoFps(const AVStream* st)
{
    AVRational r = st->r_frame_rate;
    if (r.num <= 1)
        r = st->avg_frame_rate;
    if (r.num <= 1 || r.den <= 0 || r.num > 4000)
        return {};
    return Fps(r.num, r.den);
}

MediaInfo AvProbe::probe(const QString& path) const
{
    MediaInfo info;
    info.fileName = QFileInfo(path).fileName();

    AVFormatContext* fmt = nullptr;
    QByteArray utf8 = path.toUtf8();
    if (avformat_open_input(&fmt, utf8.constData(), nullptr, nullptr) < 0) {
        info.error = QStringLiteral("failed to open file: %1").arg(QFileInfo(path).fileName());
        return info;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        info.error = QStringLiteral("failed to read stream info");
        avformat_close_input(&fmt);
        return info;
    }

    info.ok = true;
    info.formatName = QString::fromUtf8(fmt->iformat && fmt->iformat->name ? fmt->iformat->name : "?");

    int seekStream = -1;
    for (unsigned i = 0; i < fmt->nb_streams && seekStream < 0; ++i) {
        const AVMediaType t = fmt->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_VIDEO || t == AVMEDIA_TYPE_AUDIO)
            seekStream = static_cast<int>(i);
    }
    if (seekStream >= 0) {
        const int probeSeek = av_seek_frame(fmt, seekStream, 1, AVSEEK_FLAG_BACKWARD);
        info.seekable = probeSeek >= 0;
        if (info.seekable)
            av_seek_frame(fmt, seekStream, 0, AVSEEK_FLAG_BACKWARD);
    }

    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        const AVStream* st = fmt->streams[i];
        MediaStreamInfo s;
        s.index = static_cast<int>(i);
        s.codecName = QString::fromUtf8(avcodec_get_name(st->codecpar->codec_id));

        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            s.video = true;
            s.width = st->codecpar->width;
            s.height = st->codecpar->height;
            s.fps = videoFps(st);
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            s.audio = true;
            s.sampleRate = st->codecpar->sample_rate;
            s.channels = st->codecpar->ch_layout.nb_channels;
        } else {
            continue;
        }

        if (st->duration != AV_NOPTS_VALUE)
            s.durationSec = av_q2d(st->time_base) * st->duration;

        if (st->nb_frames > 0)
            s.frames = st->nb_frames;
        else if (s.durationSec > 0 && s.fps.isValid())
            s.frames = qRound(s.durationSec * s.fps.value());

        info.streams.append(std::move(s));
    }

    for (const auto& s : info.streams) {
        if (s.video && s.frames > 0) {
            info.totalFrames = s.frames;
            break;
        }
        if (s.durationSec > info.durationSec)
            info.durationSec = s.durationSec;
    }
    if (fmt->duration != AV_NOPTS_VALUE)
        info.durationSec = qMax(info.durationSec, fmt->duration / 1000000.0);

    int vIdx = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vIdx = static_cast<int>(i);
            break;
        }
    }
    if (vIdx >= 0) {
        const AVStream* vst = fmt->streams[vIdx];
        AVPacket* pkt = av_packet_alloc();
        QVector<qint64> ptsMs;
        int videoPkts = 0;
        while (videoPkts < 125 && av_read_frame(fmt, pkt) >= 0) {
            if (pkt->stream_index == vIdx) {
                ++videoPkts;
                if (pkt->pts != AV_NOPTS_VALUE)
                    ptsMs.append(av_rescale_q(pkt->pts, vst->time_base, AVRational{1, 1000}));
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
        info.streams[vIdx].vfrSuspected = looksLikeVfr(ptsMs);
        if (seekStream >= 0)
            av_seek_frame(fmt, seekStream, 0, AVSEEK_FLAG_BACKWARD);
    }

    if (info.seekable)
        av_seek_frame(fmt, seekStream, 0, AVSEEK_FLAG_BACKWARD);
    avformat_close_input(&fmt);
    return info;
}