// SPDX-License-Identifier: GPL-3.0-or-later
#include "wav.h"

#include <QFile>
#include <QSaveFile>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace wav {

namespace {

void putU32(QByteArray &b, quint32 v)
{
    char c[4];
    qToLittleEndian(v, c);
    b.append(c, 4);
}

void putU16(QByteArray &b, quint16 v)
{
    char c[2];
    qToLittleEndian(v, c);
    b.append(c, 2);
}

} // namespace

bool write(const QString &fileName, const Samples &samples, QString *error)
{
    const quint32 dataSize = quint32(samples.size() * 2);
    QByteArray b;
    b.reserve(int(44 + dataSize));
    b.append("RIFF");
    putU32(b, 36 + dataSize);
    b.append("WAVEfmt ");
    putU32(b, 16);
    putU16(b, 1);          // PCM
    putU16(b, 1);          // mono
    putU32(b, kRate);
    putU32(b, kRate * 2);  // bytes per second
    putU16(b, 2);          // block align
    putU16(b, 16);         // bits
    b.append("data");
    putU32(b, dataSize);
    for (const qint16 s : samples)
        putU16(b, quint16(s));

    QSaveFile f(fileName);
    if (!f.open(QIODevice::WriteOnly) || f.write(b) != b.size() || !f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

Samples read(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray b = f.readAll();
    if (b.size() < 12 || !b.startsWith("RIFF") || b.mid(8, 4) != "WAVE")
        return {};
    int format = 0, channels = 0, rate = 0, bits = 0;
    int pos = 12;
    QByteArray data;
    while (pos + 8 <= b.size()) {
        const QByteArray id = b.mid(pos, 4);
        const quint32 size = qFromLittleEndian<quint32>(b.constData() + pos + 4);
        const int body = pos + 8;
        if (id == "fmt " && body + 16 <= b.size()) {
            format = qFromLittleEndian<quint16>(b.constData() + body);
            channels = qFromLittleEndian<quint16>(b.constData() + body + 2);
            rate = int(qFromLittleEndian<quint32>(b.constData() + body + 4));
            bits = qFromLittleEndian<quint16>(b.constData() + body + 14);
            if (format == 0xFFFE && size >= 26) // WAVE_FORMAT_EXTENSIBLE: sub format
                format = qFromLittleEndian<quint16>(b.constData() + body + 24);
        } else if (id == "data") {
            data = b.mid(body, int(qMin<qint64>(size, b.size() - body)));
        }
        pos = body + int(size) + int(size & 1);
    }
    if (channels <= 0 || rate <= 0 || data.isEmpty())
        return {};
    const int bytes = bits / 8;
    if (bytes <= 0)
        return {};
    const int count = data.size() / bytes;
    QVector<float> v(count);
    const char *d = data.constData();
    for (int i = 0; i < count; ++i) {
        const char *p = d + i * bytes;
        float x = 0;
        if (format == 3 && bits == 32) {
            x = qFromLittleEndian<float>(p);
        } else if (format == 1 && bits == 8) {
            x = (quint8(*p) - 128) / 128.0f;
        } else if (format == 1 && bits == 16) {
            x = qFromLittleEndian<qint16>(p) / 32768.0f;
        } else if (format == 1 && bits == 24) {
            const qint32 s = (qint32(quint8(p[0])) | (qint32(quint8(p[1])) << 8) | (qint32(qint8(p[2])) << 16));
            x = s / 8388608.0f;
        } else if (format == 1 && bits == 32) {
            x = qFromLittleEndian<qint32>(p) / 2147483648.0f;
        } else {
            return {};
        }
        v[i] = x;
    }
    return toMono48k(v, channels, rate);
}

Samples toMono48k(const QVector<float> &in, int channels, int rate)
{
    if (channels <= 0 || rate <= 0)
        return {};
    const int frames = in.size() / channels;
    QVector<float> mono(frames);
    for (int i = 0; i < frames; ++i) {
        float sum = 0;
        for (int c = 0; c < channels; ++c)
            sum += in.at(i * channels + c);
        mono[i] = sum / channels;
    }
    Samples out;
    if (rate == kRate) {
        out.resize(frames);
        for (int i = 0; i < frames; ++i)
            out[i] = qint16(std::clamp(mono.at(i) * 32767.0f, -32768.0f, 32767.0f));
        return out;
    }
    // linear interpolation is good enough for speech
    const double step = double(rate) / kRate;
    const int n = int(frames / step);
    out.resize(n);
    for (int i = 0; i < n; ++i) {
        const double x = i * step;
        const int a = int(x);
        const int b2 = qMin(a + 1, frames - 1);
        const float t = float(x - a);
        const float s = mono.at(a) * (1 - t) + mono.at(b2) * t;
        out[i] = qint16(std::clamp(s * 32767.0f, -32768.0f, 32767.0f));
    }
    return out;
}

Samples clean(const Samples &in)
{
    if (in.isEmpty())
        return in;
    const int n = in.size();
    // DC offset
    double mean = 0;
    for (const qint16 s : in)
        mean += s;
    mean /= n;
    QVector<float> x(n);
    for (int i = 0; i < n; ++i)
        x[i] = float(in.at(i) - mean);

    // Loudness per 10 ms window; speech is where it rises well above the noise floor.
    const int win = kRate / 100;
    const int windows = (n + win - 1) / win;
    QVector<float> rms(windows);
    float loudest = 0;
    for (int w = 0; w < windows; ++w) {
        double sum = 0;
        const int a = w * win, b = qMin(n, a + win);
        for (int i = a; i < b; ++i)
            sum += double(x.at(i)) * x.at(i);
        rms[w] = float(std::sqrt(sum / qMax(1, b - a)));
        loudest = qMax(loudest, rms.at(w));
    }
    QVector<float> sorted = rms;
    std::sort(sorted.begin(), sorted.end());
    const float noise = sorted.at(windows / 10);
    const float threshold = qMax(noise * 3.0f, loudest * 0.04f);
    int first = 0, last = windows - 1;
    while (first < windows && rms.at(first) < threshold)
        ++first;
    while (last > first && rms.at(last) < threshold)
        --last;
    if (first >= windows)
        return {};
    // room before (consonants like "f" and "s" start quietly) and after
    const int start = qMax(0, first * win - kRate * 80 / 1000);
    const int end = qMin(n, (last + 1) * win + kRate * 120 / 1000);

    float peak = 1;
    for (int i = start; i < end; ++i)
        peak = qMax(peak, std::fabs(x.at(i)));
    const float gain = 0.89f * 32767.0f / peak; // -1 dB
    const int fade = kRate * 5 / 1000;
    Samples out(end - start);
    for (int i = start; i < end; ++i) {
        float f = 1;
        if (i - start < fade)
            f = float(i - start) / fade;
        else if (end - 1 - i < fade)
            f = float(end - 1 - i) / fade;
        out[i - start] = qint16(std::clamp(x.at(i) * gain * f, -32768.0f, 32767.0f));
    }
    return out;
}

double seconds(const Samples &s)
{
    return double(s.size()) / kRate;
}

int peakPercent(const qint16 *data, int count)
{
    int peak = 0;
    for (int i = 0; i < count; ++i)
        peak = qMax(peak, std::abs(int(data[i])));
    return peak * 100 / 32768;
}

} // namespace wav
