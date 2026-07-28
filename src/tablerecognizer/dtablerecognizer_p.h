// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef DTABLERECOGNIZER_P_H
#define DTABLERECOGNIZER_P_H

#include "dtablerecognizer.h"
#include "dtablerecognizer_global.h"

#include <QAtomicInt>
#include <QImage>
#include <QObject>
#include <QScopedPointer>
#include <QString>

#include <chrono>

#include "OrtInferenceEngine.h"
#include "TableStructureDetector.h"
#include "DtkOcrWrapper.h"
#include "CellTextMapper.h"
#include "HtmlTableBuilder.h"
#include "XlsxTableBuilder.h"
#include "Img2TableFallback.h"

D_TABLERECOGNIZER_BEGIN_NAMESPACE

class DTableRecognizerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit DTableRecognizerPrivate(DTableRecognizer *q);
    ~DTableRecognizerPrivate();

    void start(const QImage &image, std::chrono::milliseconds timeout);
    DTableResult runPipeline(QImage image, std::chrono::steady_clock::time_point deadline, const QString &xlsx);
    void emitResult(const DTableResult &result);

    DTableRecognizer *q_ptr = nullptr;

    QScopedPointer<OrtInferenceEngine> ortEngine;
    QScopedPointer<TableStructureDetector> detector;
    DtkOcrWrapper ocr;
    CellTextMapper mapper;
    Img2TableFallback fallback;

    QString xlsxOutputPath;
    QAtomicInt timedOut{0};
    QAtomicInt emitted{0};

    Q_DISABLE_COPY(DTableRecognizerPrivate)
    Q_DECLARE_PUBLIC(DTableRecognizer)
};

D_TABLERECOGNIZER_END_NAMESPACE

#endif // DTABLERECOGNIZER_P_H
