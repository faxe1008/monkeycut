#pragma once

#include <QString>
#include <QVector>

#include "core/Fps.h"

struct MediaStreamInfo
{
    int index = -1;
    QString codecName;
    bool video = false;
    bool audio = false;
    int width = 0;
    int height = 0;
    Fps fps;
    int sampleRate = 0;
    int channels = 0;
    qint64 frames = 0;
    double durationSec = 0;
    bool vfrSuspected = false;
};

struct MediaInfo
{
    bool ok = false;
    QString error;
    QString fileName;
    QString formatName;
    double durationSec = 0;
    qint64 totalFrames = 0;
    bool seekable = false;
    QVector<MediaStreamInfo> streams;

    const MediaStreamInfo* firstVideo() const
    {
        for (const auto& s : streams)
            if (s.video)
                return &s;
        return nullptr;
    }

    const MediaStreamInfo* firstAudio() const
    {
        for (const auto& s : streams)
            if (s.audio)
                return &s;
        return nullptr;
    }

    int videoStreamCount() const
    {
        int n = 0;
        for (const auto& s : streams)
            if (s.video)
                ++n;
        return n;
    }

    int audioStreamCount() const
    {
        int n = 0;
        for (const auto& s : streams)
            if (s.audio)
                ++n;
        return n;
    }
};