// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "../include/dtktablerecognizer/dtablerecognizer.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QImage>
#include <QSignalSpy>
#include <QTimer>

D_TABLERECOGNIZER_USE_NAMESPACE

TEST(ut_DTableRecognizer, invalidImageEmitsFailure)
{
    DTableRecognizer recognizer;
    QSignalSpy spy(&recognizer, &DTableRecognizer::recognitionDone);

    recognizer.recognizeAsync(QImage(), std::chrono::seconds(5));

    EXPECT_TRUE(spy.wait(2000));
    ASSERT_EQ(spy.count(), 1);
    const auto result = qvariant_cast<DTableResult>(spy.takeFirst().at(0));
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage.toStdString(), "输入图片无效");
}
