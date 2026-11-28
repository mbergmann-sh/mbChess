// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the tutor speaks up: a picture of the instructor on the left (for a
// particularly stupid move a special one), on the right a short explanation of what is
// wrong with the move and what is better, and the choice what to do.
#pragma once

#include "tutor.h"

#include <QDialog>

class TutorDialog : public QDialog
{
    Q_OBJECT
public:
    enum Choice { TakeBack = 0, PlayBest, PlayAnyway };

    TutorDialog(const Tutor::Verdict &verdict, const QString &pieceLetters, QWidget *parent = nullptr);
    Choice choice() const { return m_choice; }

    // The explanation (rich text), also used by the tests.
    static QString explanation(const Tutor::Verdict &verdict, const QString &pieceLetters);

private:
    Choice m_choice = TakeBack;
};
