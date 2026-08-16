#include "GopScanner.h"

#include "ffmpeg_c.h"

#include <QFileInfo>
#include <QString>

GopMap scanGopMap(const QString& path)
{
    GopMap map;

    AVFormatContext* fmt = nullptr;
    QByteArray utf8 = path.toUtf8();
    if (avformat_open_input(&fmt, utf8.constData(), nullptr, nullptr) < 0)
        return map;
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return map;
    }

    int vIdx = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vIdx = static_cast<int>(i);
            break;
        }
    }
    if (vIdx < 0) {
        avformat_close_input(&fmt);
        return map;
    }

    AVPacket* pkt = av_packet_alloc();
    qint64 count = 0;
    while (av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index == vIdx) {
            if (pkt->flags & AV_PKT_FLAG_KEY)
                map.keyframes.append(count);
            ++count;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    avformat_close_input(&fmt);

    map.frameCount = count;
    map.valid = !map.keyframes.isEmpty();
    return map;
}