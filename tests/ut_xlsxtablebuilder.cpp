// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "../include/dtktablerecognizer/dtablecell.h"
#include "../include/dtktablerecognizer/dtablerecognizer_global.h"
#include "../src/tablerecognizer/XlsxTableBuilder.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QFileInfo>
#include <QString>

D_TABLERECOGNIZER_USE_NAMESPACE

TEST(ut_XlsxTableBuilder, writesToTempPath)
{
    QList<DTableCell> cells;
    DTableCell a;
    a.row = 0;
    a.col = 0;
    a.text = QStringLiteral("Hello");
    DTableCell b;
    b.row = 0;
    b.col = 1;
    b.text = QStringLiteral("World");
    cells << a << b;

    const QString path = XlsxTableBuilder::build(cells, QString());
    ASSERT_FALSE(path.isEmpty()) << "xlsx build should succeed with libxlsxwriter";

    EXPECT_TRUE(QFileInfo::exists(path)) << "xlsx file should exist at: " << path.toStdString();
    EXPECT_GT(QFileInfo(path).size(), qint64(0));

    QFile::remove(path);
}

TEST(ut_XlsxTableBuilder, writesToGivenPath)
{
    const QString path = QStringLiteral("/tmp/ut_tablerec_test_out.xlsx");
    QFile::remove(path);

    QList<DTableCell> cells;
    DTableCell c;
    c.row = 0;
    c.col = 0;
    c.rowSpan = 2;
    c.colSpan = 2;
    c.text = QStringLiteral("Merged");
    cells << c;

    const QString out = XlsxTableBuilder::build(cells, path);
    EXPECT_EQ(out.toStdString(), path.toStdString());
    EXPECT_TRUE(QFileInfo::exists(path));

    QFile::remove(path);
}
