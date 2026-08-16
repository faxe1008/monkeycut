#include "VfrDetect.h"

#include <QtGlobal>

bool looksLikeVfr(const QVector<qint64>& ptsMillis)
{
    QVector<qint64> deltas;
    for (int i = 1; i < ptsMillis.size(); ++i)
        deltas.append(ptsMillis[i] - ptsMillis[i - 1]);

    QVector<qint64> distinct;
    for (const auto d : deltas) {
        bool found = false;
        for (const auto x : distinct) {
            if (qAbs(d - x) <= 1) {
                found = true;
                break;
            }
        }
        if (!found)
            distinct.append(d);
    }
    return distinct.size() > 2;
}