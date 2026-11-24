// SPDX-License-Identifier: GPL-3.0-or-later
#include "gaviotatb.h"

#include "gtb-probe.h"

#include <QByteArray>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>

namespace mbc::gaviota {

namespace {

QMutex s_mutex;
bool s_open = false;
QString s_folder;
const char **s_paths = nullptr;

unsigned tbPiece(PieceType t)
{
    switch (t) {
    case Pawn: return tb_PAWN;
    case Knight: return tb_KNIGHT;
    case Bishop: return tb_BISHOP;
    case Rook: return tb_ROOK;
    case Queen: return tb_QUEEN;
    case King: return tb_KING;
    default: return tb_NOPIECE;
    }
}

void closeLocked()
{
    if (s_open) {
        tbcache_done();
        tb_done();
    }
    if (s_paths)
        s_paths = tbpaths_done(s_paths);
    s_open = false;
    s_folder.clear();
}

} // namespace

int Availability::completeUpTo() const
{
    int n = 0;
    for (int p = 3; p <= 5 && complete[p]; ++p)
        n = p;
    return n;
}

bool open(const QString &folderPath, int cacheMegabytes)
{
    QMutexLocker lock(&s_mutex);
    const QString wanted = folderPath.isEmpty() ? QString() : QDir::cleanPath(folderPath);
    if (s_open && wanted == s_folder)
        return tb_availability() != 0;
    closeLocked();
    if (wanted.isEmpty() || !QDir(wanted).exists())
        return false;
    const QByteArray path = QDir::toNativeSeparators(wanted).toLocal8Bit();
    s_paths = tbpaths_init();
    s_paths = tbpaths_add(s_paths, path.constData()); // the library keeps a copy
    tb_init(0, tb_CP4, s_paths);
    tbcache_init(size_t(qMax(1, cacheMegabytes)) * 1024 * 1024, 96);
    s_open = true;
    s_folder = wanted;
    return tb_availability() != 0;
}

void close()
{
    QMutexLocker lock(&s_mutex);
    closeLocked();
}

bool isOpen()
{
    QMutexLocker lock(&s_mutex);
    return s_open && tb_availability() != 0;
}

QString folder()
{
    QMutexLocker lock(&s_mutex);
    return s_folder;
}

Availability availability()
{
    QMutexLocker lock(&s_mutex);
    Availability a;
    if (!s_open)
        return a;
    const unsigned bits = tb_availability();
    for (int p = 3; p <= 5; ++p) {
        const int shift = 2 * (p - 3);
        a.some[p] = (bits >> shift) & 1u;
        a.complete[p] = (bits >> (shift + 1)) & 1u;
    }
    return a;
}

int pieceCount(const Position &pos)
{
    int n = 0;
    for (int sq = 0; sq < 64; ++sq)
        if (pos.pieceAt(sq) != NoPiece)
            ++n;
    return n;
}

bool probe(const Position &pos, Result *result)
{
    if (pos.castlingRights() != 0 || pieceCount(pos) > 5)
        return false;
    // square lists end with tb_NOSQUARE, piece lists with tb_NOPIECE; mbChess and
    // Gaviota both count the squares a1 = 0, b1 = 1, ..., h8 = 63
    unsigned wSq[17], bSq[17];
    unsigned char wPc[17], bPc[17];
    int nw = 0, nb = 0;
    for (int sq = 0; sq < 64; ++sq) {
        const Piece p = pos.pieceAt(sq);
        if (p == NoPiece)
            continue;
        if (colorOf(p) == White) {
            wSq[nw] = unsigned(sq);
            wPc[nw++] = static_cast<unsigned char>(tbPiece(typeOf(p)));
        } else {
            bSq[nb] = unsigned(sq);
            bPc[nb++] = static_cast<unsigned char>(tbPiece(typeOf(p)));
        }
    }
    wSq[nw] = tb_NOSQUARE;
    wPc[nw] = tb_NOPIECE;
    bSq[nb] = tb_NOSQUARE;
    bPc[nb] = tb_NOPIECE;
    const unsigned stm = pos.sideToMove() == White ? tb_WHITE_TO_MOVE : tb_BLACK_TO_MOVE;
    const unsigned ep = pos.enPassantSquare() >= 0 ? unsigned(pos.enPassantSquare()) : unsigned(tb_NOSQUARE);

    QMutexLocker lock(&s_mutex);
    if (!s_open)
        return false;
    unsigned info = tb_UNKNOWN, plies = 0;
    if (!tb_probe_hard(stm, ep, tb_NOCASTLE, wSq, bSq, wPc, bPc, &info, &plies))
        return false;
    Result r;
    switch (info) {
    case tb_DRAW: r.kind = Result::Draw; break;
    case tb_WMATE: r.kind = Result::WhiteMates; r.plies = int(plies); break;
    case tb_BMATE: r.kind = Result::BlackMates; r.plies = int(plies); break;
    default: return false;
    }
    if (result)
        *result = r;
    return true;
}

} // namespace mbc::gaviota
