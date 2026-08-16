#pragma once

#include <QString>

#include "core/MediaInfo.h"

class AvProbe
{
public:
    MediaInfo probe(const QString& path) const;
};