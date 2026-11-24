// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - move sounds and spoken move announcements.
//
// A sound set (themes::soundFolder) holds one WAV file per word: a..h, 1..8, pawn,
// knight, bishop, rook, queen, king, takes, check, mate, stalemate, draw, o-o, o-o-o,
// welcome (greeting at start-up, optional) and the effects tap (a move) and hit (a
// capture). Missing effects are taken from the
// built-in set "effects" (synthesised wood sounds). Everything is optional: without Qt
// Multimedia the player is silent and isAvailable() is false.
//
// The WAV files are decoded into memory once (48 kHz, mono, 16 bit). The words of a move
// are joined into one piece of sound, with short pauses in between, and played through a
// single audio output, so only one audio stream is ever open.
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>

class QBuffer;
class QIODevice;
#ifdef MBCHESS_HAVE_SOUND
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioSink;
#else
class QAudioOutput;
#endif
#endif

class SoundPlayer : public QObject
{
    Q_OBJECT
public:
    explicit SoundPlayer(QObject *parent = nullptr);
    ~SoundPlayer() override;

    static bool isAvailable();

    void setEffectsEnabled(bool on) { m_effects = on; }
    void setSpeechEnabled(bool on) { m_speech = on; }
    bool effectsEnabled() const { return m_effects; }
    bool speechEnabled() const { return m_speech; }
    // The set used for announcements, e.g. "thorsten-de", "libritts-en" or "amichess".
    void setSpeechSet(const QString &name);
    // Decodes the sounds of a set in advance and opens the audio output (both quick).
    void preload(const QString &folder);
    QString speechFolder() const { return m_speechFolder; }
    QString effectsFolder() const { return m_effectsFolder; }

    // A move in English SAN ("Nxf3+", "O-O", "e8=Q#").
    void playMove(const QString &san);
    // A single word such as "draw" or "stalemate" (only with speech on), after what is
    // still being said.
    void announce(const QString &word);

    // The words spoken for a move (for tests and debugging).
    static QStringList wordsForMove(const QString &san);
    // A WAV file as 48 kHz mono 16-bit samples (empty if it cannot be read).
    static QByteArray decodeWav(const QByteArray &file);

    static constexpr int kSampleRate = 48000;

private:
    QString fileFor(const QString &folder, const QString &word) const;
    const QByteArray &samples(const QString &path);
    void play(const QByteArray &pcm, bool append);
    bool ensureOutput();

    bool m_effects = true;
    bool m_speech = false;
    QString m_speechFolder;
    QString m_effectsFolder;
    QHash<QString, QByteArray> m_samples;   // path -> decoded samples
    QByteArray m_data;                      // what is being played
    QBuffer *m_buffer = nullptr;
    bool m_outputFailed = false;
#ifdef MBCHESS_HAVE_SOUND
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioSink *m_output = nullptr;
#else
    QAudioOutput *m_output = nullptr;
#endif
#endif
};
