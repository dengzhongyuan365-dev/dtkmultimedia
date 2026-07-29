// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "XlsxTableBuilder.h"

#ifdef DTK_HAVE_XLSXWRITER
#include <xlsxwriter.h>
#endif

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtLogging>

D_TABLERECOGNIZER_BEGIN_NAMESPACE

static QString resolveOutputPath(const QString &outPath)
{
    if (!outPath.isEmpty())
        return outPath;
    const QString tmpl = QStringLiteral("tablerec_%1_XXXXXX.xlsx")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss")));
    QFileInfo info(QDir::temp(), tmpl);
    return info.absoluteFilePath();
}

QString XlsxTableBuilder::build(const QList<DTableCell> &cells, const QString &outPath)
{
#ifndef DTK_HAVE_XLSXWRITER
    Q_UNUSED(cells)
    Q_UNUSED(outPath)
    // libxlsxwriter 不可用：Excel 输出降级为空路径，不阻断整体识别流程。
    qWarning("XlsxTableBuilder: libxlsxwriter not available, Excel output disabled");
    return QString();
#else
    const QString path = resolveOutputPath(outPath);
    const QByteArray pathBytes = path.toUtf8();

    lxw_workbook *workbook = workbook_new(pathBytes.constData());
    if (!workbook)
        return QString();

    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, nullptr);
    lxw_format *format = workbook_add_format(workbook);
    format_set_border(format, LXW_BORDER_THIN);
    format_set_align(format, LXW_ALIGN_LEFT);
    format_set_align(format, LXW_ALIGN_VERTICAL_TOP);

    bool ok = true;
    for (const DTableCell &cell : cells) {
        const int row = std::max(0, cell.row);
        const int col = std::max(0, cell.col);
        const int rs = std::max(1, cell.rowSpan);
        const int cs = std::max(1, cell.colSpan);
        const QByteArray text = cell.text.toUtf8();
        if (rs > 1 || cs > 1) {
            const lxw_error err = worksheet_merge_range(worksheet, row, col,
                                                        row + rs - 1, col + cs - 1,
                                                        text.constData(), format);
            if (err != LXW_NO_ERROR)
                ok = false;
        } else {
            const lxw_error err = worksheet_write_string(worksheet, row, col,
                                                         text.constData(), format);
            if (err != LXW_NO_ERROR)
                ok = false;
        }
    }

    const lxw_error closeErr = workbook_close(workbook);
    if (closeErr != LXW_NO_ERROR || !ok) {
        QFile::remove(path);
        return QString();
    }
    return path;
#endif
}

D_TABLERECOGNIZER_END_NAMESPACE
