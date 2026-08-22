#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QObject>

#include "core/MediaInfo.h"

#include <atomic>
#include <deque>

class QThread;
class QTimer;
class AudioBuffer;
class DecodeThread;

class Player : public QObject
{
    Q_OBJECT
public:
    enum class State { Stopped, Playing, Paused };

    explicit Player(QObject* parent = nullptr);
    ~Player();

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    bool open(const QString& path, const MediaInfo& info);
    void close();

    bool isOpen() const;
    State state() const;
    qint64 currentFrame() const;
    qint64 totalFrames() const;
    int width() const;
    int height() const;
    const Fps& fps() const;
    bool hasAudio() const;

    void play();
    void pause();
    void stepFrame(qint64 n);
    void seekFrame(qint64 frame, bool exact);

    void setSpeed(qreal multiplier);
    qreal speed() const;

signals:
    void frameAvailable(const QImage& image); // next displayable frame
    void frameDisplayed(qint64 frame);         // frame actually shown
    void positionChanged(qint64 frame);       // current frame (transport/scrub)
    void stateChanged(Player::State state);
    void errorOccurred(const QString& message);

private:
    friend class DecodeThread;

    void enterState(State s);
    void decodeExited();
    void decodeReachedEof();
    void displayTick();

    qint64 frameToMs(qint64 frame) const;
    qint64 msToFrame(qint64 ms) const;
    qint64 clockFrame() const;
    qint64 clampFrame(qint64 frame) const;

    void pushPending(qint64 frame, const QImage& img);
    void postPosition(qint64 frame);
    void postError(const QString& message);
    void seekFailed();

    QString m_path;
    MediaInfo m_info;
    Fps m_fps;
    int m_width = 0;
    int m_height = 0;
    qint64 m_totalFrames = 0;
    bool m_hasAudio = false;
    State m_state = State::Stopped;
    qreal m_speed = 1.0;

    QThread* m_worker = nullptr;
    QTimer* m_tick = nullptr;
    QElapsedTimer m_elapsed;
    AudioBuffer* m_audioBuf = nullptr;

    std::atomic<qint64> m_currentFrame{0};
    // Tracks the most recently requested seek/step target (which may still
    // be in flight on the decode thread). stepFrame() bases its next target
    // on this rather than m_currentFrame, since m_currentFrame only settles
    // once a previously requested seek has fully completed; using it as the
    // base while a seek is still pending would let repeated rapid steps
    // (e.g. holding or repeatedly clicking the next/prev frame button)
    // compute a stale/lower target and appear to jump backward.
    std::atomic<qint64> m_lastRequestedFrame{0};
    std::atomic<qint64> m_clockMs{0};
    std::atomic<qint64> m_seekTarget{0};
    std::atomic<bool> m_seekPending{false};
    std::atomic<bool> m_exactSeek{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_workerEof{false};
    std::atomic<qint64> m_lastShown{-1};

    struct Pending
    {
        qint64 frame;
        QImage image;
    };
    QMutex m_pendingMutex;
    std::deque<Pending> m_pending;
    qint64 m_lastTickMs = 0;
};
