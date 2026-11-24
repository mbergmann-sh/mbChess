// SPDX-License-Identifier: GPL-3.0-or-later
#include "soundplayer.h"

#include "themes.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QtEndian>

#ifdef MBCHESS_HAVE_SOUND
#include <QAudioFormat>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#else
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#endif
#endif

namespace {

QString pieceWord(QChar c)
{
    switch (c.unicode()) {
    case 'K': return QStringLiteral("king");
    case 'Q': return QStringLiteral("queen");
    case 'R': return QStringLiteral("rook");
    case 'B': return QStringLiteral("bishop");
    case 'N': return QStringLiteral("knight");
    default: break;
    }
    return QString();
}

QByteArray silence(int ms)
{
    return QByteArray(SoundPlayer::kSampleRate * ms / 1000 * 2, '\0');
}

const int kWordPauseMs = 110;   // between spoken words: keeps letters and digits apart
const int kEffectPauseMs = 40;  // after the knock, before the first word

} // namespace

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent)
    , m_buffer(new QBuffer(this))
{
    m_effectsFolder = themes::soundFolder(QStringLiteral("effects"));
    m_speechFolder = themes::soundFolder(themes::defaultSounds());
}

SoundPlayer::~SoundPlayer()
{
#ifdef MBCHESS_HAVE_SOUND
    if (m_output)
        m_output->stop();
#endif
}

bool SoundPlayer::isAvailable()
{
#ifdef MBCHESS_HAVE_SOUND
    // MBCHESS_NO_SOUND=1 switches all sound off, e.g. when the audio system makes trouble.
    static const bool disabled = qEnvironmentVariableIsSet("MBCHESS_NO_SOUND");
    return !disabled;
#else
    return false;
#endif
}

void SoundPlayer::setSpeechSet(const QString &name)
{
    const QString folder = themes::soundFolder(name);
    m_speechFolder = folder.isEmpty() ? themes::soundFolder(themes::defaultSounds()) : folder;
}

void SoundPlayer::preload(const QString &folder)
{
    if (!isAvailable())
        return;
    // After the window is up: decoding takes a few milliseconds per folder.
    QTimer::singleShot(500, this, [this, folder] {
        const QStringList files = QDir(folder).entryList({ QStringLiteral("*.wav") }, QDir::Files);
        for (const QString &f : files)
            samples(folder + QLatin1Char('/') + f);
        ensureOutput();
    });
}

QByteArray SoundPlayer::decodeWav(const QByteArray &d)
{
    if (d.size() < 44 || !d.startsWith("RIFF") || d.mid(8, 4) != "WAVE")
        return QByteArray();
    const auto u16 = [&d](int pos) { return qFromLittleEndian<quint16>(d.constData() + pos); };
    const auto u32 = [&d](int pos) { return qFromLittleEndian<quint32>(d.constData() + pos); };
    int channels = 0, rate = 0, bits = 0;
    QByteArray raw;
    int pos = 12;
    while (pos + 8 <= d.size()) {
        const QByteArray id = d.mid(pos, 4);
        const qint64 len = u32(pos + 4);
        if (id == "fmt " && pos + 24 <= d.size()) {
            if (u16(pos + 8) != 1) // PCM only
                return QByteArray();
            channels = u16(pos + 10);
            rate = int(u32(pos + 12));
            bits = u16(pos + 22);
        } else if (id == "data") {
            raw = d.mid(pos + 8, int(qMin<qint64>(len, d.size() - pos - 8)));
            break;
        }
        pos += 8 + int(len) + int(len & 1);
    }
    if (channels < 1 || rate <= 0 || (bits != 8 && bits != 16) || raw.isEmpty())
        return QByteArray();

    // to mono floats
    const int frameBytes = channels * bits / 8;
    const int frames = raw.size() / frameBytes;
    QVector<float> mono(frames);
    for (int i = 0; i < frames; ++i) {
        float sum = 0;
        for (int c = 0; c < channels; ++c) {
            const char *p = raw.constData() + i * frameBytes + c * bits / 8;
            sum += bits == 8 ? (quint8(*p) - 128) / 128.0f : qFromLittleEndian<qint16>(p) / 32768.0f;
        }
        mono[i] = sum / channels;
    }
    // to 48 kHz (linear interpolation; the sets are 48 kHz already, AmiChess is not)
    const int outFrames = int(qint64(frames) * kSampleRate / rate);
    QByteArray out(outFrames * 2, Qt::Uninitialized);
    for (int i = 0; i < outFrames; ++i) {
        const double src = double(i) * rate / kSampleRate;
        const int a = qMin(int(src), frames - 1);
        const int b = qMin(a + 1, frames - 1);
        const float v = mono[a] + float(src - a) * (mono[b] - mono[a]);
        qToLittleEndian<qint16>(qint16(qBound(-32767.0f, v * 32767.0f, 32767.0f)), out.data() + 2 * i);
    }
    return out;
}

const QByteArray &SoundPlayer::samples(const QString &path)
{
    auto it = m_samples.find(path);
    if (it == m_samples.end()) {
        QFile f(path);
        it = m_samples.insert(path, f.open(QIODevice::ReadOnly) ? decodeWav(f.readAll()) : QByteArray());
    }
    return it.value();
}

QStringList SoundPlayer::wordsForMove(const QString &san)
{
    QStringList words;
    QString s = san;
    const bool mate = s.endsWith(QLatin1Char('#'));
    const bool check = s.endsWith(QLatin1Char('+'));
    while (s.endsWith(QLatin1Char('#')) || s.endsWith(QLatin1Char('+')) || s.endsWith(QLatin1Char('!'))
           || s.endsWith(QLatin1Char('?')))
        s.chop(1);

    if (s == QLatin1String("O-O-O") || s == QLatin1String("0-0-0")) {
        words << QStringLiteral("o-o-o");
    } else if (s == QLatin1String("O-O") || s == QLatin1String("0-0")) {
        words << QStringLiteral("o-o");
    } else {
        for (int i = 0; i < s.size(); ++i) {
            const QChar c = s.at(i);
            if (c == QLatin1Char('x'))
                words << QStringLiteral("takes");
            else if (c >= QLatin1Char('a') && c <= QLatin1Char('h'))
                words << QString(c);
            else if (c >= QLatin1Char('1') && c <= QLatin1Char('8'))
                words << QString(c);
            else if (!pieceWord(c).isEmpty())
                words << pieceWord(c); // the moving piece, or the promotion piece after '='
        }
    }
    if (mate)
        words << QStringLiteral("mate");
    else if (check)
        words << QStringLiteral("check");
    return words;
}

QString SoundPlayer::fileFor(const QString &folder, const QString &word) const
{
    QString path = folder + QLatin1Char('/') + word + QStringLiteral(".wav");
    if (QFileInfo::exists(path))
        return path;
    path = m_effectsFolder + QLatin1Char('/') + word + QStringLiteral(".wav");
    return QFileInfo::exists(path) ? path : QString();
}

void SoundPlayer::playMove(const QString &san)
{
    if (!isAvailable() || (!m_effects && !m_speech))
        return;
    QByteArray pcm;
    if (m_effects) {
        const QString path = fileFor(m_effectsFolder, san.contains(QLatin1Char('x')) ? QStringLiteral("hit")
                                                                                   : QStringLiteral("tap"));
        if (!path.isEmpty())
            pcm += samples(path);
    }
    if (m_speech) {
        bool firstWord = true;
        for (const QString &w : wordsForMove(san)) {
            const QString path = fileFor(m_speechFolder, w);
            if (path.isEmpty())
                continue;
            if (!pcm.isEmpty())
                pcm += silence(firstWord ? kEffectPauseMs : kWordPauseMs);
            pcm += samples(path);
            firstWord = false;
        }
    }
    // A new move cuts off what is still being said about the previous one.
    play(pcm, false);
}

void SoundPlayer::announce(const QString &word)
{
    if (!isAvailable() || !m_speech)
        return;
    const QString path = fileFor(m_speechFolder, word);
    if (!path.isEmpty())
        play(silence(kWordPauseMs * 2) + samples(path), true);
}

bool SoundPlayer::ensureOutput()
{
#ifdef MBCHESS_HAVE_SOUND
    if (m_output)
        return true;
    if (m_outputFailed)
        return false;
    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setChannelCount(1);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    format.setSampleFormat(QAudioFormat::Int16);
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull() || !device.isFormatSupported(format)) {
        m_outputFailed = true;
        return false;
    }
    m_output = new QAudioSink(device, format, this);
#else
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec(QStringLiteral("audio/pcm"));
    const QAudioDeviceInfo device = QAudioDeviceInfo::defaultOutputDevice();
    if (device.isNull() || !device.isFormatSupported(format)) {
        m_outputFailed = true;
        return false;
    }
    m_output = new QAudioOutput(device, format, this);
#endif
    return true;
#else
    return false;
#endif
}

void SoundPlayer::play(const QByteArray &pcm, bool append)
{
#ifdef MBCHESS_HAVE_SOUND
    if (pcm.isEmpty() || !ensureOutput())
        return;
    QByteArray data = pcm;
    if (append && m_buffer->isOpen() && m_buffer->pos() < m_data.size())
        data = m_data.mid(int(m_buffer->pos())) + pcm; // after what is still being played
    m_output->stop();
    m_buffer->close();
    // Some audio systems drop the last few hundred milliseconds when a stream ends;
    // trailing silence keeps the last word whole.
    m_data = data + silence(400);
    m_buffer->setData(m_data);
    m_buffer->open(QIODevice::ReadOnly);
    m_output->start(m_buffer);
#else
    Q_UNUSED(pcm)
    Q_UNUSED(append)
#endif
}
