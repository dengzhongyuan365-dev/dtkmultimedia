// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TableStructureDetector.h"

#include "OrtInferenceEngine.h"

#include <QDebug>
#include <QMap>
#include <QRegularExpression>
#include <QString>

D_TABLERECOGNIZER_BEGIN_NAMESPACE

namespace {

// SLANet_plus 结构词表（代表性子集，与 Python 参考实现一致）。
// 索引即 token id；未列出的 id 视为普通文本/未知 token。
const QStringList &vocab()
{
    static const QStringList v = {
        QStringLiteral("<pad>"),      // 0
        QStringLiteral("<sos>"),      // 1
        QStringLiteral("<eos>"),      // 2
        QStringLiteral("<table>"),    // 3
        QStringLiteral("</table>"),   // 4
        QStringLiteral("<thead>"),    // 5
        QStringLiteral("</thead>"),   // 6
        QStringLiteral("<tbody>"),    // 7
        QStringLiteral("</tbody>"),   // 8
        QStringLiteral("<tr>"),       // 9
        QStringLiteral("</tr>"),      // 10
        QStringLiteral("<td>"),       // 11 — 无属性单元格
        QStringLiteral("<td"),        // 12 — 带 colspan/rowspan 的起始 token
        QStringLiteral(">"),          // 13
        QStringLiteral("</td>"),      // 14
        QStringLiteral("colspan=\""), // 15
        QStringLiteral("rowspan=\""), // 16
        QStringLiteral("\""),         // 17
        QStringLiteral(" "),          // 18
    };
    return v;
}

// 解析形如 "<td colspan=\"2\" rowspan=\"3\">" 的属性串，提取 span。
void parseTdSpan(const QString &attrs, int &rowSpan, int &colSpan)
{
    rowSpan = 1;
    colSpan = 1;
    QRegularExpression reCol(QStringLiteral("colspan=\"(\\d+)\""));
    QRegularExpression reRow(QStringLiteral("rowspan=\"(\\d+)\""));
    QRegularExpressionMatch mCol = reCol.match(attrs);
    QRegularExpressionMatch mRow = reRow.match(attrs);
    if (mCol.hasMatch())
        colSpan = mCol.captured(1).toInt();
    if (mRow.hasMatch())
        rowSpan = mRow.captured(1).toInt();
    if (colSpan < 1)
        colSpan = 1;
    if (rowSpan < 1)
        rowSpan = 1;
}

} // namespace

QStringList TableStructureDetector::vocabulary()
{
    return vocab();
}

TableStructureDetector::TableStructureDetector(OrtInferenceEngine *engine)
    : m_engine(engine)
{
}

TableStructureDetector::~TableStructureDetector() = default;

bool TableStructureDetector::available() const
{
    return m_engine && m_engine->isLoaded();
}

bool TableStructureDetector::detect(const QImage &image, QList<DetectedCell> &cells, QString &error)
{
    cells.clear();
    if (!available()) {
        error = QStringLiteral("表格结构检测器不可用：ORT 模型未加载");
        return false;
    }
    if (image.isNull()) {
        error = QStringLiteral("输入图片无效");
        return false;
    }

    constexpr int kInputSize = 488;
    std::vector<float> input = preprocess(image, kInputSize);
    if (input.empty()) {
        error = QStringLiteral("图片预处理失败");
        return false;
    }

    const std::vector<int64_t> shape = {1, 3, kInputSize, kInputSize};
    std::vector<std::vector<float>> outputs = m_engine->run(input, shape);
    if (outputs.empty()) {
        error = m_engine->lastError();
        if (error.isEmpty())
            error = QStringLiteral("ORT 推理无输出");
        return false;
    }
    if (outputs.size() < 2) {
        error = QStringLiteral("ORT 输出节点不足（需结构与 bbox 两个）");
        return false;
    }

    // 结构 token：第一个输出，按 [T, V] 逐时间步 argmax。
    const auto &structOut = outputs[0];
    const int V = vocab().size();
    std::vector<int> structIds;
    if (V > 0 && structOut.size() % size_t(V) == 0) {
        const size_t T = structOut.size() / size_t(V);
        for (size_t t = 0; t < T; ++t) {
            const float *row = structOut.data() + t * V;
            int best = 0;
            float bestVal = row[0];
            for (int v = 1; v < V; ++v) {
                if (row[v] > bestVal) {
                    bestVal = row[v];
                    best = v;
                }
            }
            structIds.push_back(best);
        }
    } else {
        // 退化：直接当作已 argmax 的 id 序列使用。
        for (size_t t = 0; t < structOut.size(); ++t)
            structIds.push_back(static_cast<int>(structOut[t]));
    }

    const auto &bboxOut = outputs[1];
    cells = decodeStructure(structIds, bboxOut, image.size());
    if (cells.isEmpty()) {
        error = QStringLiteral("未识别到表格");
        return false;
    }
    return true;
}

std::vector<float> TableStructureDetector::preprocess(const QImage &image, int inputSize)
{
    if (image.isNull() || inputSize <= 0)
        return {};

    QImage scaled = image.scaled(inputSize, inputSize, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_RGB888);
    if (scaled.isNull())
        return {};

    constexpr float mean[3] = {0.485f, 0.456f, 0.406f};
    constexpr float std[3] = {0.229f, 0.224f, 0.225f};

    const int bytesPerLine = scaled.bytesPerLine();
    const uchar *bits = scaled.constBits();
    std::vector<float> tensor(static_cast<size_t>(3) * inputSize * inputSize, 0.0f);
    const size_t planeSize = static_cast<size_t>(inputSize) * inputSize;

    for (int y = 0; y < inputSize; ++y) {
        const uchar *line = bits + size_t(y) * bytesPerLine;
        for (int x = 0; x < inputSize; ++x) {
            const int base = x * 3;
            const float r = line[base] / 255.0f;
            const float g = line[base + 1] / 255.0f;
            const float b = line[base + 2] / 255.0f;
            tensor[x * inputSize + y] = (r - mean[0]) / std[0];
            tensor[planeSize + x * inputSize + y] = (g - mean[1]) / std[1];
            tensor[2 * planeSize + x * inputSize + y] = (b - mean[2]) / std[2];
        }
    }
    return tensor;
}

QList<DetectedCell> TableStructureDetector::decodeStructure(const std::vector<int> &structureIds,
                                                            const std::vector<float> &bboxes,
                                                            const QSize &imageSize)
{
    const QStringList &v = vocab();
    const int V = v.size();

    QList<DetectedCell> cells;
    int row = 0;
    int col = 0;
    // spanRows[c] = 该列还被上方 rowspan 占用的行数。
    QMap<int, int> spanRows;
    int cellIndex = 0;
    bool inTd = false;       // 正在累积 <td ...> 属性
    QString attrBuf;

    auto advanceRow = [&]() {
        // 行结束：递减各列的 rowspan 占用计数。
        for (int c : spanRows.keys()) {
            int left = spanRows[c] - 1;
            if (left <= 0)
                spanRows.remove(c);
            else
                spanRows[c] = left;
        }
        ++row;
        col = 0;
    };

    auto emitCell = [&](int rowSpan, int colSpan) {
        // 跳过被上方 rowspan 覆盖的列。
        while (spanRows.value(col, 0) > 0)
            ++col;
        const int startCol = col;

        DetectedCell cell;
        cell.row = row;
        cell.col = startCol;
        cell.rowSpan = rowSpan;
        cell.colSpan = colSpan;

        if (size_t(cellIndex + 1) * 4 <= bboxes.size()) {
            const size_t bi = size_t(cellIndex) * 4;
            const float x1 = bboxes[bi];
            const float y1 = bboxes[bi + 1];
            const float x2 = bboxes[bi + 2];
            const float y2 = bboxes[bi + 3];
            cell.bbox = QRectF(QPointF(x1, y1), QPointF(x2, y2));
        }
        cells.append(cell);

        for (int c = startCol; c < startCol + colSpan; ++c) {
            if (rowSpan > 1)
                spanRows[c] = rowSpan - 1;
            else
                spanRows.remove(c);
        }
        col += colSpan;
        ++cellIndex;
    };

    for (size_t i = 0; i < structureIds.size(); ++i) {
        const int id = structureIds[i];
        if (id < 0 || id >= V)
            continue;
        const QString &tok = v.at(id);

        if (inTd) {
            // 累积属性直到 ">" 闭合。
            if (tok == QStringLiteral(">")) {
                int rowSpan = 1, colSpan = 1;
                parseTdSpan(attrBuf, rowSpan, colSpan);
                emitCell(rowSpan, colSpan);
                attrBuf.clear();
                inTd = false;
            } else if (tok == QStringLiteral("</td>")) {
                // 容错：属性未闭合但遇到 </td>，按无属性单元格处理。
                int rowSpan = 1, colSpan = 1;
                if (!attrBuf.isEmpty())
                    parseTdSpan(attrBuf, rowSpan, colSpan);
                emitCell(rowSpan, colSpan);
                attrBuf.clear();
                inTd = false;
            } else {
                attrBuf += tok;
            }
            continue;
        }

        if (tok == QStringLiteral("<tr>")) {
            // 新行开始。
        } else if (tok == QStringLiteral("</tr>")) {
            advanceRow();
        } else if (tok == QStringLiteral("<td>")) {
            emitCell(1, 1);
        } else if (tok == QStringLiteral("<td")) {
            inTd = true;
            attrBuf.clear();
        }
        // 其它 token（<table>/<thead>/<tbody>/</td>/<pad>/<sos>/<eos> 等）忽略结构语义。
    }

    // 容错：流结束时仍在累积属性，按已收集属性闭合。
    if (inTd && !attrBuf.isEmpty()) {
        int rowSpan = 1, colSpan = 1;
        parseTdSpan(attrBuf, rowSpan, colSpan);
        emitCell(rowSpan, colSpan);
    }

    // 归一化 bbox（0~1）缩放到原图尺寸。
    for (DetectedCell &cell : cells) {
        if (imageSize.width() > 0 && imageSize.height() > 0) {
            const QRectF nb = cell.bbox;
            cell.bbox = QRectF(QPointF(nb.left() * imageSize.width(),
                                       nb.top() * imageSize.height()),
                               QPointF(nb.right() * imageSize.width(),
                                       nb.bottom() * imageSize.height()));
        }
    }

    return cells;
}

D_TABLERECOGNIZER_END_NAMESPACE
