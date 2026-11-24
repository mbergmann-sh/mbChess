// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess voice recorder - WAV files and simple sound processing.
// Everything works on mono 16-bit samples at 48 kHz, the format mbChess plays.
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace wav {

constexpr int kRate = 48000;

using Samples = QVector<qint16>;

// Writes 48 kHz mono 16-bit PCM.
bool write(const QString &fileName, const Samples &samples, QString *error = nullptr);
// Reads PCM (8/16/24/32 bit integer or 32-bit float, any channels and rate) and
// converts it to 48 kHz mono. Empty on failure.
Samples read(const QString &fileName);

// Interleaved input -> mono at 48 kHz.
Samples toMono48k(const QVector<float> &interleaved, int channels, int rate);

// Cuts silence at both ends (with a little room), removes DC offset, raises the
// level so that the loudest peak is at about -1 dB, and fades in/out over 5 ms.
Samples clean(const Samples &in);

double seconds(const Samples &s);
int peakPercent(const qint16 *data, int count);

} // namespace wav
