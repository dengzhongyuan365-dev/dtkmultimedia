// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "../src/tablerecognizer/TableStructureDetector.h"
#include "../src/tablerecognizer/OrtInferenceEngine.h"

#include <gtest/gtest.h>

D_TABLERECOGNIZER_USE_NAMESPACE

// 词表索引（与 TableStructureDetector.cpp 内 vocab() 一致，从 ONNX 模型元数据提取）：
// 0:<sos> 1:<eos> 2:<thead> 3:</thead> 4:<tbody> 5:</tbody>
// 6:<tr> 7:</tr> 8:<td 9:> 10:</td>
// 11..29: colspan="2" .. colspan="20"
// 30..48: rowspan="2" .. rowspan="20"
// 49: <td></td>
static std::vector<int> ids(std::initializer_list<int> lst)
{
    return std::vector<int>(lst);
}

// 解码一个 2x2 表格，验证行列。
// 使用 <td >（id 8,9）表示普通单元格，stride=4 的 bbox。
TEST(ut_TableStructureDetector, decodeSimpleGrid)
{
    // <tbody><tr><td><td></tr><tr><td><td></tr></tbody>
    std::vector<int> structIds = ids({4, 6, 8, 9, 8, 9, 7, 6, 8, 9, 8, 9, 7, 5});
    std::vector<float> bboxes = {
        0.0f, 0.0f, 0.5f, 0.5f,
        0.5f, 0.0f, 1.0f, 0.5f,
        0.0f, 0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 1.0f, 1.0f,
    };

    QList<DetectedCell> cells = TableStructureDetector::decodeStructure(structIds, bboxes, QSize(100, 100));
    ASSERT_EQ(cells.size(), 4);
    EXPECT_EQ(cells[0].row, 0);
    EXPECT_EQ(cells[0].col, 0);
    EXPECT_EQ(cells[1].row, 0);
    EXPECT_EQ(cells[1].col, 1);
    EXPECT_EQ(cells[2].row, 1);
    EXPECT_EQ(cells[2].col, 0);
    EXPECT_EQ(cells[3].row, 1);
    EXPECT_EQ(cells[3].col, 1);
    EXPECT_FLOAT_EQ(cells[0].bbox.left(), 0.0f);
    EXPECT_FLOAT_EQ(cells[3].bbox.right(), 100.0f);
}

// 使用 <td></td>（id 49）自闭合空单元格解码 1x2 表格。
TEST(ut_TableStructureDetector, decodeSelfClosingCell)
{
    // <tbody><tr><td></td><td></td></tr></tbody>
    std::vector<int> structIds = ids({4, 6, 49, 49, 7, 5});
    std::vector<float> bboxes = {
        0.0f, 0.0f, 0.5f, 1.0f,
        0.5f, 0.0f, 1.0f, 1.0f,
    };

    QList<DetectedCell> cells = TableStructureDetector::decodeStructure(structIds, bboxes, QSize(50, 50));
    ASSERT_EQ(cells.size(), 2);
    EXPECT_EQ(cells[0].col, 0);
    EXPECT_EQ(cells[1].col, 1);
}

// 解码含 colspan="2" 的单元格，验证 span 与后续单元格列号。
TEST(ut_TableStructureDetector, decodeColspan)
{
    // <tbody><tr><td colspan="2"> <td></tr></tbody>
    // 8:<td 11:colspan="2" 9:> 8:<td 9:>
    std::vector<int> structIds = ids({4, 6, 8, 11, 9, 8, 9, 7, 5});
    std::vector<float> bboxes = {
        0.0f, 0.0f, 1.0f, 0.5f,
        0.0f, 0.5f, 1.0f, 1.0f,
    };

    QList<DetectedCell> cells = TableStructureDetector::decodeStructure(structIds, bboxes, QSize(50, 50));
    ASSERT_EQ(cells.size(), 2);
    EXPECT_EQ(cells[0].colSpan, 2);
    EXPECT_EQ(cells[0].col, 0);
    EXPECT_EQ(cells[1].col, 2); // 第二个单元格跳过被 colspan 覆盖的列
}

// 解码含 rowspan="2" 的单元格，验证跨行。
TEST(ut_TableStructureDetector, decodeRowspan)
{
    // <tbody><tr><td rowspan="2"><td></tr><tr><td></tr></tbody>
    // 8:<td 30:rowspan="2" 9:> 8:<td 9:> 7:</tr> 6:<tr> 8:<td 9:> 7:</tr>
    std::vector<int> structIds = ids({4, 6, 8, 30, 9, 8, 9, 7, 6, 8, 9, 7, 5});
    std::vector<float> bboxes = {
        0.0f, 0.0f, 0.5f, 1.0f,
        0.5f, 0.0f, 1.0f, 0.5f,
        0.5f, 0.5f, 1.0f, 1.0f,
    };

    QList<DetectedCell> cells = TableStructureDetector::decodeStructure(structIds, bboxes, QSize(50, 50));
    ASSERT_EQ(cells.size(), 3);
    EXPECT_EQ(cells[0].rowSpan, 2);
    EXPECT_EQ(cells[0].row, 0);
    EXPECT_EQ(cells[0].col, 0);
    // 第二行的第一个单元格应跳过 col 0（被 rowspan 占用），落在 col 1。
    EXPECT_EQ(cells[2].row, 1);
    EXPECT_EQ(cells[2].col, 1);
}

// 验证 8 值 bbox stride（4 个多边形顶点）的解码。
TEST(ut_TableStructureDetector, decodeBboxStride8)
{
    // <tbody><tr><td><td></tr></tbody>
    std::vector<int> structIds = ids({4, 6, 8, 9, 8, 9, 7, 5});
    // 2 个单元格，每个 8 个值（4 顶点：x1,y1,x2,y2,x3,y3,x4,y4）
    std::vector<float> bboxes = {
        0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f, 0.5f,
        0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f,
    };

    QList<DetectedCell> cells = TableStructureDetector::decodeStructure(structIds, bboxes, QSize(100, 100), 8);
    ASSERT_EQ(cells.size(), 2);
    EXPECT_FLOAT_EQ(cells[0].bbox.left(), 0.0f);
    EXPECT_FLOAT_EQ(cells[0].bbox.right(), 50.0f);
    EXPECT_FLOAT_EQ(cells[1].bbox.left(), 50.0f);
    EXPECT_FLOAT_EQ(cells[1].bbox.right(), 100.0f);
}

// 验证 available() 在无引擎/未加载模型时返回 false。
TEST(ut_TableStructureDetector, notAvailableWithoutLoadedModel)
{
    OrtInferenceEngine engine;
    TableStructureDetector detector(&engine);
    EXPECT_FALSE(detector.available());
}

// 验证词表大小为 50（与模型 V=50 一致）。
TEST(ut_TableStructureDetector, vocabularySize)
{
    EXPECT_FALSE(TableStructureDetector::vocabulary().isEmpty());
    EXPECT_EQ(TableStructureDetector::vocabulary().size(), 50);
}
