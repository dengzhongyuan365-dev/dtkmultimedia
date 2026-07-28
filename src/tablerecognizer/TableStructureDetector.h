// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef TABLESTRUCTUREDETECTOR_H
#define TABLESTRUCTUREDETECTOR_H

#include "dtablerecognizer_global.h"
#include "tabletypes.h"

#include <QImage>
#include <QList>

D_TABLERECOGNIZER_BEGIN_NAMESPACE

class OrtInferenceEngine;

// SLANet_plus 表格结构检测：预处理 -> ORT 推理 -> 结构 token 解码 -> 单元格 bbox。
class TableStructureDetector
{
public:
    explicit TableStructureDetector(OrtInferenceEngine *engine);
    ~TableStructureDetector();

    // 对图片做表格结构检测；成功返回 true 并填充 cells。
    bool detect(const QImage &image, QList<DetectedCell> &cells, QString &error);

    // 引擎已加载模型时返回 true。
    bool available() const;

    // ===== 以下为可单独测试的纯逻辑接口 =====

    // QImage -> NCHW float 张量（resize 到 inputSize，归一化 mean/std）。
    // 返回 CHW 顺序的 float 数据，shape = {1, 3, inputSize, inputSize}。
    static std::vector<float> preprocess(const QImage &image, int inputSize);

    // 结构 token id 序列 + bbox 浮点序列 -> DetectedCell 列表。
    // bbox 为 [x1,y1,x2,y2] 归一化坐标（0~1），按结构序列中单元格出现顺序排列。
    static QList<DetectedCell> decodeStructure(const std::vector<int> &structureIds,
                                               const std::vector<float> &bboxes,
                                               const QSize &imageSize);

    // 结构词表（代表性子集，覆盖 SLANet_plus 常用结构 token）。
    static QStringList vocabulary();

private:
    OrtInferenceEngine *m_engine;
};

D_TABLERECOGNIZER_END_NAMESPACE

#endif // TABLESTRUCTUREDETECTOR_H
