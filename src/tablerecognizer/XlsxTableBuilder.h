// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XLSXTABLEBUILDER_H
#define XLSXTABLEBUILDER_H

#include "dtablerecognizer_global.h"
#include "dtablecell.h"

#include <QString>

D_TABLERECOGNIZER_BEGIN_NAMESPACE

// 结构化单元格 -> Excel(.xlsx)。按 (row,col) 写入，合并区域用 worksheet_merge_range。
// outPath 为空时写入临时目录，返回实际路径；失败返回空字符串。
class XlsxTableBuilder
{
public:
    static QString build(const QList<DTableCell> &cells, const QString &outPath);
};

D_TABLERECOGNIZER_END_NAMESPACE

#endif // XLSXTABLEBUILDER_H
