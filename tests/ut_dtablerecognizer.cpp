// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "../include/dtktablerecognizer/dtablerecognizer.h"
#include "../src/tablerecognizer/OrtInferenceEngine.h"
#include "../src/tablerecognizer/TableStructureDetector.h"
#include "../src/tablerecognizer/DtkOcrWrapper.h"
#include "../src/tablerecognizer/Img2TableFallback.h"
#include "../src/tablerecognizer/XlsxTableBuilder.h"

#include <gtest/gtest.h>
#include <QImage>
#include <QSignalSpy>
#include <stubext.h>

#include <chrono>
#include <thread>

D_TABLERECOGNIZER_USE_NAMESPACE

// 在事件循环中等待 recognitionDone 信号，返回结果。
static DTableResult waitForDone(QSignalSpy &spy, int timeoutMs = 5000)
{
    spy.wait(timeoutMs);
    if (spy.isEmpty())
        return DTableResult{};
    return qvariant_cast<DTableResult>(spy.takeFirst().at(0));
}

// 用例1：无效图片直接返回失败（同步路径）。
TEST(ut_DTableRecognizer, invalidImageEmitsFailure)
{
    DTableRecognizer recognizer;
    QSignalSpy spy(&recognizer, &DTableRecognizer::recognitionDone);
    recognizer.recognizeAsync(QImage());
    const DTableResult result = waitForDone(spy, 2000);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage.toStdString(), "输入图片无效");
}

// 用例2：超时返回失败。stub detect 阻塞，使超时定时器先触发。
TEST(ut_DTableRecognizer, timeoutReturnsFailure)
{
    DTableRecognizer recognizer;
    QSignalSpy spy(&recognizer, &DTableRecognizer::recognitionDone);

    stub_ext::StubExt stub;
    // 主路径不可用。
    stub.set_lamda(ADDR(TableStructureDetector, available), []() { return false; });
    // 降级路径阻塞 300ms，确保 10ms 超时先触发。
    stub.set_lamda(ADDR(Img2TableFallback, detect),
                   [](Img2TableFallback *, const QImage &, QList<DetectedCell> &cells, QString &) {
                       std::this_thread::sleep_for(std::chrono::milliseconds(300));
                       return false;
                   });

    QImage image(100, 100, QImage::Format_RGB32);
    image.fill(Qt::white);
    recognizer.recognizeAsync(image, std::chrono::milliseconds(10));
    const DTableResult result = waitForDone(spy, 3000);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.contains(QStringLiteral("超时")));
}

// 用例3：降级路径 source 字段。主路径不可用，img2table + OCR + xlsx 全部 stub 成功。
TEST(ut_DTableRecognizer, degradationPathSetsSource)
{
    DTableRecognizer recognizer;
    QSignalSpy spy(&recognizer, &DTableRecognizer::recognitionDone);

    stub_ext::StubExt stub;
    stub.set_lamda(ADDR(TableStructureDetector, available), []() { return false; });
    stub.set_lamda(ADDR(Img2TableFallback, detect),
                   [](Img2TableFallback *, const QImage &, QList<DetectedCell> &cells, QString &) {
                       DetectedCell c;
                       c.row = 0;
                       c.col = 0;
                       c.bbox = QRectF(0, 0, 50, 50);
                       cells.append(c);
                       return true;
                   });
    stub.set_lamda(ADDR(DtkOcrWrapper, recognize),
                   [](DtkOcrWrapper *, const QImage &, QList<OcrTextBox> &boxes, QString &) {
                       OcrTextBox box;
                       box.bbox = QRectF(10, 10, 20, 20);
                       box.text = QStringLiteral("A");
                       boxes.append(box);
                       return true;
                   });
    stub.set_lamda(ADDR(XlsxTableBuilder, build),
                   [](const QList<DTableCell> &, const QString &) {
                       return QStringLiteral("/tmp/ut_test_out.xlsx");
                   });

    QImage image(100, 100, QImage::Format_RGB32);
    image.fill(Qt::white);
    recognizer.recognizeAsync(image, std::chrono::seconds(10));
    const DTableResult result = waitForDone(spy, 5000);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.source.toStdString(), "img2table");
    ASSERT_EQ(result.cells.size(), 1);
    EXPECT_EQ(result.cells[0].text.toStdString(), "A");
}
