#include "Player.h"

#include "ffmpeg_c.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QtGlobal>

namespace
{
constexpr int kAudioRate = 48000;
constexpr int kAudioChannels = 2;
constexpr qint64 kBytesPerMs = qint64(kAudioRate) * kAudioChannels * 2 / 1000;
constexpr int kMaxAudioMs = 1500;
constexpr int kMaxPendingVideo = 4;
constexpr int kSeekScanLimit = 5000;
}

class AudioBuffer
{
public:
    void push(qint64 ptsMs, const QByteArray& data)
    {
        QMutexLocker l(&m_mut);
        qint64 have = 0;
        for (const auto& c : m_q)
            have += qint64(c.data.size()) - c.consumed;
        while (!m_q.empty() && have + qint64(data.size()) > qint64(kMaxAudioMs * kBytesPerMs)) {
            have -= qint64(m_q.front().data.size()) - m_q.front().consumed;
            m_q.pop_front();
        }
        m_q.push_back({ptsMs, data, 0});
    }

    int bufferedMs()
    {
        QMutexLocker l(&m_mut);
        qint64 bytes = 0;
        for (const auto& c : m_q)
            bytes += qint64(c.data.size()) - c.consumed;
        return int(bytes / kBytesPerMs);
    }

    void reset()
    {
        QMutexLocker l(&m_mut);
        m_q.clear();
    }

private:
    struct Chunk
    {
        qint64 ptsMs;
        QByteArray data;
        int consumed = 0;
    };
    QMutex m_mut;
    std::deque<Chunk> m_q;
};

namespace
{
qint64 ptsToMs(qint64 pts, const AVRational& tb)
{
    return av_rescale_q(pts, tb, AVRational{1, 1000});
}
}

class DecodeThread : public QThread
{
public:
    explicit DecodeThread(Player* owner)
        : m_owner(owner)
    {
    }

protected:
    void run() override
    {
        AVPacket* pkt = av_packet_alloc();
        AVFrame* vframe = av_frame_alloc();
        AVFrame* aframe = av_frame_alloc();

        if (!setup(m_owner->m_path)) {
            m_owner->postError(QStringLiteral("failed to open file in decoder thread"));
        } else {
            bool eof = false;
            while (!m_owner->m_stopping.load()) {
                if (m_owner->m_seekPending.exchange(false)) {
                    performSeek(vframe);
                    eof = false;
                    continue;
                }
                if (!m_owner->m_playing.load() || eof) {
                    msleep(eof ? 10 : 5);
                    continue;
                }

                const int r = av_read_frame(m_fmt, pkt);
                if (r < 0) {
                    eof = true;
                    flushVideoQueue(vframe);
                    flushAudioQueue(aframe);
                    QMetaObject::invokeMethod(m_owner, &Player::decodeReachedEof,
                                              Qt::QueuedConnection);
                    continue;
                }

                if (pkt->stream_index == m_vIdx) {
                    avcodec_send_packet(m_vc, pkt);
                    for (;;) {
                        const int rr = avcodec_receive_frame(m_vc, vframe);
                        if (rr == AVERROR(EAGAIN) || rr < 0)
                            break;
                        frameToScreen(vframe, -1);
                    }
                } else if (m_aIdx >= 0 && pkt->stream_index == m_aIdx && m_ac) {
                    avcodec_send_packet(m_ac, pkt);
                    for (;;) {
                        const int rr = avcodec_receive_frame(m_ac, aframe);
                        if (rr == AVERROR(EAGAIN) || rr < 0)
                            break;
                        frameToAudio(aframe);
                    }
                }
                av_packet_unref(pkt);

                bool backpressure =
                    m_owner->m_audioBuf && m_owner->m_audioBuf->bufferedMs() > kMaxAudioMs;
                if (!backpressure) {
                    QMutexLocker l(&m_owner->m_pendingMutex);
                    backpressure = m_owner->m_pending.size() >= kMaxPendingVideo
                        && m_owner->m_pending.back().frame > m_owner->clockFrame() + 8;
                }
                if (backpressure)
                    msleep(10);
            }
        }

        av_packet_free(&pkt);
        av_frame_free(&vframe);
        av_frame_free(&aframe);
        sws_freeContext(m_sws);
        swr_free(&m_swr);
        avcodec_free_context(&m_vc);
        avcodec_free_context(&m_ac);
        avformat_close_input(&m_fmt);

        QMetaObject::invokeMethod(m_owner, &Player::decodeExited, Qt::QueuedConnection);
    }

private:
    bool setup(const QString& path)
    {
        QByteArray utf8 = path.toUtf8();
        if (avformat_open_input(&m_fmt, utf8.constData(), nullptr, nullptr) < 0)
            return false;
        if (avformat_find_stream_info(m_fmt, nullptr) < 0)
            return false;

        for (unsigned i = 0; i < m_fmt->nb_streams; ++i) {
            const AVMediaType t = m_fmt->streams[i]->codecpar->codec_type;
            if (t == AVMEDIA_TYPE_VIDEO && m_vIdx < 0)
                m_vIdx = int(i);
            else if (t == AVMEDIA_TYPE_AUDIO && m_aIdx < 0)
                m_aIdx = int(i);
        }
        if (m_vIdx < 0)
            return false;

        {
            const AVStream* vst = m_fmt->streams[m_vIdx];
            m_startMs = vst->start_time != AV_NOPTS_VALUE
                ? ptsToMs(vst->start_time, vst->time_base)
                : 0;
        }

        AVCodecParameters* vp = m_fmt->streams[m_vIdx]->codecpar;
        const AVCodec* dc = avcodec_find_decoder(vp->codec_id);
        if (!dc)
            return false;
        m_vc = avcodec_alloc_context3(dc);
        avcodec_parameters_to_context(m_vc, vp);
        if (avcodec_open2(m_vc, dc, nullptr) < 0)
            return false;

        if (m_aIdx >= 0) {
            AVCodecParameters* ap = m_fmt->streams[m_aIdx]->codecpar;
            const AVCodec* dcA = avcodec_find_decoder(ap->codec_id);
            if (dcA) {
                m_ac = avcodec_alloc_context3(dcA);
                avcodec_parameters_to_context(m_ac, ap);
                if (avcodec_open2(m_ac, dcA, nullptr) < 0)
                    avcodec_free_context(&m_ac);
            }
        }
        return true;
    }

    bool frameToScreen(AVFrame* frame, qint64 needFrame, qint64* outFrame = nullptr)
    {
        if (frame->width <= 0 || frame->height <= 0)
            return false;
        if (!m_sws || m_swsW != frame->width || m_swsH != frame->height
            || m_swsFmt != static_cast<AVPixelFormat>(frame->format)) {
            sws_freeContext(m_sws);
            m_sws = sws_getContext(frame->width, frame->height,
                                   static_cast<AVPixelFormat>(frame->format),
                                   frame->width, frame->height, AV_PIX_FMT_RGB32,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!m_sws)
                return false;
            m_swsW = frame->width;
            m_swsH = frame->height;
            m_swsFmt = static_cast<AVPixelFormat>(frame->format);
        }

        QImage img(frame->width, frame->height, QImage::Format_RGB32);
        uint8_t* rows[1] = {img.bits()};
        int stride[1] = {int(img.bytesPerLine())};
        sws_scale(m_sws, frame->data, frame->linesize, 0, frame->height, rows, stride);

        const AVStream* vst = m_fmt->streams[m_vIdx];
        qint64 ms = frame->pts != AV_NOPTS_VALUE
            ? ptsToMs(frame->pts, vst->time_base) - m_startMs
            : m_lastVideoMs + qint64(1000.0 / m_owner->fps().value());
        m_lastVideoMs = ms;

        const qint64 fr = m_owner->msToFrame(ms);
        if (outFrame)
            *outFrame = fr;
        if (needFrame >= 0 && fr < needFrame)
            return false;
        m_owner->pushPending(fr, img);
        m_owner->postPosition(fr);
        return true;
    }

    void frameToAudio(AVFrame* frame)
    {
        if (!m_ac || !m_owner->m_audioBuf)
            return;
        const AVStream* ast = m_fmt->streams[m_aIdx];

        if (!m_swr || m_swrInFmt != static_cast<AVSampleFormat>(frame->format)
            || m_swrInRate != ast->codecpar->sample_rate
            || av_channel_layout_compare(&m_swrInLayout, &ast->codecpar->ch_layout) != 0) {
            swr_free(&m_swr);
            m_swr = nullptr;
            AVChannelLayout out;
            av_channel_layout_default(&out, kAudioChannels);
            if (swr_alloc_set_opts2(&m_swr, &out, AV_SAMPLE_FMT_S16, kAudioRate,
                                    &ast->codecpar->ch_layout,
                                    static_cast<AVSampleFormat>(frame->format),
                                    ast->codecpar->sample_rate, 0, nullptr)
                < 0
                || (m_swr && swr_init(m_swr) < 0)) {
                swr_free(&m_swr);
                m_swr = nullptr;
                av_channel_layout_uninit(&out);
                return;
            }
            av_channel_layout_uninit(&out);
            m_swrInFmt = static_cast<AVSampleFormat>(frame->format);
            m_swrInRate = ast->codecpar->sample_rate;
            av_channel_layout_copy(&m_swrInLayout, &ast->codecpar->ch_layout);
        }

        size_t cap = static_cast<size_t>(frame->nb_samples) * 4 + 4096;
        uint8_t* outBuf = static_cast<uint8_t*>(av_malloc(cap * 2 * sizeof(int16_t)));
        if (!outBuf)
            return;
        const int outSamples = swr_convert(m_swr, &outBuf, 1,
                                           frame->data,
                                           frame->nb_samples);
        if (outSamples > 0) {
            qint64 ms = frame->pts != AV_NOPTS_VALUE
                ? ptsToMs(frame->pts, ast->time_base) - m_startMs
                : 0;
            if (ms < 0)
                ms = 0;
            m_owner->m_audioBuf->push(ms, QByteArray(reinterpret_cast<char*>(outBuf),
                                                     int(outSamples * 2 * sizeof(int16_t))));
        }
        av_freep(&outBuf);
    }

    void performSeek(AVFrame* vframe)
    {
        const qint64 target = m_owner->m_seekTarget.load();
        const bool exact = m_owner->m_exactSeek.exchange(false);

        const AVStream* vst = m_fmt->streams[m_vIdx];
        const qint64 baseTs =
            av_rescale_q(m_owner->frameToMs(target) + m_startMs,
                         AVRational{1, 1000}, vst->time_base);

        AVPacket* pkt = av_packet_alloc();
        AVFrame* tmp = av_frame_alloc();
        bool done = false;

        // Some demuxers (mpegts ...) land one packet late when seeking exactly
        // onto a packet boundary, skipping the keyframe itself. Seek one tick
        // before the requested time and, for exact seeks, retry with a growing
        // backward margin if the first decoded frame already lies past the
        // target (overshoot).
        for (int attempt = 0; attempt < 3 && !done
             && !m_owner->m_stopping.load();
             ++attempt) {
            const qint64 marginMs =
                (attempt == 0) ? 0 : (attempt == 1 ? 500 : 2000);
            qint64 ts = baseTs - 1
                - av_rescale_q(marginMs, AVRational{1, 1000}, vst->time_base);
            if (ts < 0)
                ts = 0;

            flushVideoQueue(vframe);
            flushAudioQueue(nullptr);
            if (m_owner->m_audioBuf)
                m_owner->m_audioBuf->reset();
            {
                QMutexLocker l(&m_owner->m_pendingMutex);
                m_owner->m_pending.clear();
            }
            avcodec_flush_buffers(m_vc);
            if (m_ac)
                avcodec_flush_buffers(m_ac);

            if (av_seek_frame(m_fmt, m_vIdx, ts, AVSEEK_FLAG_BACKWARD) < 0)
                break;

            bool overshoot = false;
            bool firstFrame = true;
            for (int guard = 0; guard < kSeekScanLimit && !done && !overshoot
                 && !m_owner->m_stopping.load();
                 ++guard) {
                const int r = av_read_frame(m_fmt, pkt);
                if (r < 0)
                    break;
                if (pkt->stream_index == m_vIdx) {
                    avcodec_send_packet(m_vc, pkt);
                    for (;;) {
                        const int rr = avcodec_receive_frame(m_vc, tmp);
                        if (rr == AVERROR(EAGAIN) || rr < 0)
                            break;
                        qint64 fr = -1;
                        if (frameToScreen(tmp, -1, &fr)) {
                            if (exact) {
                                if (firstFrame && fr > target)
                                    overshoot = true;
                                else if (fr >= target)
                                    done = true;
                            } else {
                                done = true;
                            }
                            firstFrame = false;
                            if (done)
                                break;
                        }
                    }
                }
                if (done || overshoot)
                    break;
                av_packet_unref(pkt);
            }
            if (done)
                break;
            if (!overshoot)
                break; // unreadable or failed: don't retry forever
        }

        av_packet_free(&pkt);
        av_frame_free(&tmp);
        if (!done && !m_owner->m_stopping.load())
            m_owner->seekFailed();
    }

    void flushVideoQueue(AVFrame* vframe)
    {
        if (!m_vc)
            return;
        avcodec_send_packet(m_vc, nullptr);
        for (;;) {
            const int r = avcodec_receive_frame(m_vc, vframe);
            if (r == AVERROR(EAGAIN) || r < 0)
                break;
            frameToScreen(vframe, -1);
        }
    }

    void flushAudioQueue(AVFrame* aframe)
    {
        if (!m_ac || !aframe)
            return;
        avcodec_send_packet(m_ac, nullptr);
        for (int guard = 0; guard < 64; ++guard) {
            const int r = avcodec_receive_frame(m_ac, aframe);
            if (r == AVERROR(EAGAIN) || r < 0)
                break;
        }
    }

    Player* m_owner;
    AVFormatContext* m_fmt = nullptr;
    int m_vIdx = -1;
    int m_aIdx = -1;
    qint64 m_startMs = 0;
    AVCodecContext* m_vc = nullptr;
    AVCodecContext* m_ac = nullptr;
    SwsContext* m_sws = nullptr;
    int m_swsW = 0;
    int m_swsH = 0;
    AVPixelFormat m_swsFmt = AV_PIX_FMT_NONE;
    SwrContext* m_swr = nullptr;
    AVSampleFormat m_swrInFmt = AV_SAMPLE_FMT_NONE;
    int m_swrInRate = 0;
    AVChannelLayout m_swrInLayout = {};
    qint64 m_lastVideoMs = 0;
};

Player::Player(QObject* parent)
    : QObject(parent)
{
}

Player::~Player()
{
    close();
}

bool Player::isOpen() const
{
    return m_worker != nullptr;
}

Player::State Player::state() const
{
    return m_state;
}

qint64 Player::currentFrame() const
{
    return m_currentFrame.load();
}

qint64 Player::totalFrames() const
{
    return m_totalFrames;
}

int Player::width() const
{
    return m_width;
}

int Player::height() const
{
    return m_height;
}

const Fps& Player::fps() const
{
    return m_fps;
}

bool Player::hasAudio() const
{
    return m_hasAudio;
}

qint64 Player::frameToMs(qint64 frame) const
{
    if (!m_fps.isValid())
        return 0;
    return frame * qint64(1000) * m_fps.den / m_fps.num;
}

qint64 Player::msToFrame(qint64 ms) const
{
    if (!m_fps.isValid())
        return 0;
    const __int128 v = static_cast<__int128>(ms) * m_fps.num + 500 * m_fps.den;
    return static_cast<qint64>(v / (1000 * m_fps.den));
}

qint64 Player::clockFrame() const
{
    return msToFrame(m_clockMs.load());
}

qint64 Player::clampFrame(qint64 frame) const
{
    const qint64 maxF = m_totalFrames > 0 ? m_totalFrames - 1 : (1 << 40);
    if (frame < 0)
        return 0;
    if (frame > maxF)
        return maxF;
    return frame;
}

void Player::enterState(State s)
{
    if (s == m_state)
        return;
    m_state = s;
    Q_EMIT stateChanged(s);
}

bool Player::open(const QString& path, const MediaInfo& info)
{
    close();
    const MediaStreamInfo* v = info.firstVideo();
    if (!v || !v->fps.isValid()) {
        Q_EMIT errorOccurred(QStringLiteral("no usable video stream"));
        return false;
    }

    m_path = path;
    m_info = info;
    m_fps = v->fps;
    m_width = v->width;
    m_height = v->height;
    m_totalFrames = info.totalFrames > 0
        ? info.totalFrames
        : qint64(info.durationSec * v->fps.value());
    m_hasAudio = info.firstAudio() != nullptr;
    m_currentFrame = 0;
    m_lastShown.store(-1);
    m_clockMs = 0;
    m_workerEof = false;
    m_state = State::Stopped;

    m_audioBuf = new AudioBuffer;
    m_worker = new DecodeThread(this);
    m_worker->start();

    m_tick = new QTimer(this);
    m_tick->setInterval(25);
    connect(m_tick, &QTimer::timeout, this, &Player::displayTick);
    m_elapsed.start();
    m_lastTickMs = m_elapsed.elapsed();
    m_tick->start();

    seekFrame(0, true);
    return true;
}

void Player::close()
{
    if (m_tick) {
        m_tick->stop();
        delete m_tick;
        m_tick = nullptr;
    }

    m_playing = false;
    if (m_worker) {
        m_stopping = true;
        if (!m_worker->wait(3000)) {
            m_worker->terminate();
            m_worker->wait(1000);
        }
        m_stopping = false;
        QCoreApplication::sendPostedEvents(this);
        delete m_worker;
        m_worker = nullptr;
    }
    m_workerEof = false;

    delete m_audioBuf;
    m_audioBuf = nullptr;
    {
        QMutexLocker l(&m_pendingMutex);
        m_pending.clear();
    }
    m_currentFrame = 0;
    m_lastShown.store(-1);
    m_state = State::Stopped;
}

void Player::play()
{
    if (!isOpen() || m_state == State::Playing)
        return;
    if (m_workerEof.load()) {
        m_workerEof = false;
        m_seekTarget = 0;
        m_exactSeek = true;
        m_seekPending = true;
    }
    m_clockMs = frameToMs(m_currentFrame.load());
    m_lastTickMs = m_elapsed.elapsed();
    m_playing = true;
    enterState(State::Playing);
}

void Player::pause()
{
    if (m_state != State::Playing)
        return;
    m_playing = false;
    enterState(State::Paused);
}

void Player::stepFrame(qint64 n)
{
    if (n == 0 || !isOpen())
        return;
    if (m_state == State::Playing)
        pause();
    seekFrame(m_currentFrame.load() + n, true);
}

void Player::seekFrame(qint64 frame, bool exact)
{
    if (!isOpen())
        return;
    m_seekTarget.store(clampFrame(frame));
    m_exactSeek = exact;
    m_seekPending = true;
}

void Player::setSpeed(qreal multiplier)
{
    m_speed = qBound(0.1, multiplier, 8.0);
}

qreal Player::speed() const
{
    return m_speed;
}

void Player::decodeExited()
{
    if (m_worker) {
        delete m_worker;
        m_worker = nullptr;
    }
    delete m_audioBuf;
    m_audioBuf = nullptr;
    m_playing = false;
    if (m_state != State::Stopped) {
        m_state = State::Stopped;
        Q_EMIT stateChanged(State::Stopped);
    }
}

void Player::decodeReachedEof()
{
    m_workerEof = true;
    if (m_state == State::Playing) {
        m_playing = false;
        enterState(State::Paused);
    }
}

void Player::displayTick()
{
    if (!isOpen())
        return;

    const qint64 now = m_elapsed.elapsed();
    qint64 targetFrame = -1;
    if (m_playing.load()) {
        const qint64 dt = now - m_lastTickMs;
        m_clockMs += qint64(double(dt) * m_speed);
        targetFrame = clockFrame();
    }
    m_lastTickMs = now;

    QImage toShow;
    qint64 shownFrame = -1;
    {
        QMutexLocker l(&m_pendingMutex);
        const Pending* show = nullptr;
        if (targetFrame >= 0) {
            for (auto it = m_pending.rbegin(); it != m_pending.rend(); ++it) {
                if (it->frame <= targetFrame) {
                    show = &*it;
                    break;
                }
            }
        } else if (!m_pending.empty()) {
            show = &m_pending.back();
        }
        if (show && show->frame != m_lastShown.load()) {
            shownFrame = show->frame;
            toShow = show->image;
            while (!m_pending.empty() && m_pending.front().frame <= shownFrame)
                m_pending.pop_front();
            m_pending.push_back({shownFrame, toShow});
        }
    }
    if (shownFrame >= 0) {
        m_lastShown = shownFrame;
        m_currentFrame = shownFrame;
        Q_EMIT frameAvailable(toShow);
        Q_EMIT positionChanged(shownFrame);
    }
}

void Player::postPosition(qint64 frame)
{
    if (frame < 0)
        return;
    if (m_playing.load())
        m_clockMs = frameToMs(frame);
    m_currentFrame = frame;
    Q_EMIT positionChanged(frame);
}

void Player::postError(const QString& message)
{
    Q_EMIT errorOccurred(message);
}

void Player::seekFailed()
{
    Q_EMIT errorOccurred(QStringLiteral("seek failed"));
}

void Player::pushPending(qint64 frame, const QImage& img)
{
    QMutexLocker l(&m_pendingMutex);
    while (m_pending.size() >= kMaxPendingVideo)
        m_pending.pop_front();
    m_pending.push_back({frame, img});
}