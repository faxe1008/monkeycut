#pragma once

#include <QString>

#include "core/GopMap.h"

namespace GopCache
{
    bool load(const QString& videoPath, GopMap* out);
    void save(const QString& videoPath, const GopMap& map);
}