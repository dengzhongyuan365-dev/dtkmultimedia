// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "OrtInferenceEngine.h"

#include <onnxruntime_cxx_api.h>

#include <QDebug>
#include <QFileInfo>

D_TABLERECOGNIZER_BEGIN_NAMESPACE

class OrtInferenceEngine::Impl
{
public:
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "dtk6tablerecognizer"};
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;
    QStringList inputNames;
    QStringList outputNames;
    QString lastError;
    bool loaded = false;
};

OrtInferenceEngine::OrtInferenceEngine()
    : d(std::make_unique<Impl>())
{
}

OrtInferenceEngine::~OrtInferenceEngine() = default;

bool OrtInferenceEngine::loadModel(const QString &modelPath)
{
    d->loaded = false;
    d->session.reset();
    d->lastError.clear();
    d->inputNames.clear();
    d->outputNames.clear();

    if (modelPath.isEmpty()) {
        d->lastError = QStringLiteral("模型路径为空");
        return false;
    }
    const QFileInfo info(modelPath);
    if (!info.exists() || !info.isFile()) {
        d->lastError = QStringLiteral("模型文件不存在: %1").arg(modelPath);
        return false;
    }

    try {
        Ort::SessionOptions sessionOptions;
        // 抑制 ONNX schema 重复注册的刷屏日志（ORT_LOGGING_LEVEL_FATAL=4）。
        sessionOptions.SetLogSeverityLevel(4);
        d->session = std::make_unique<Ort::Session>(d->env, modelPath.toUtf8().constData(),
                                                    sessionOptions);
        const size_t inCount = d->session->GetInputCount();
        for (size_t i = 0; i < inCount; ++i) {
            auto name = d->session->GetInputNameAllocated(i, d->allocator);
            d->inputNames << QString::fromUtf8(name.get());
        }
        const size_t outCount = d->session->GetOutputCount();
        for (size_t i = 0; i < outCount; ++i) {
            auto name = d->session->GetOutputNameAllocated(i, d->allocator);
            d->outputNames << QString::fromUtf8(name.get());
        }
        d->loaded = true;
        return true;
    } catch (const Ort::Exception &e) {
        d->lastError = QStringLiteral("ORT 加载失败: %1").arg(QString::fromUtf8(e.what()));
        d->session.reset();
        return false;
    } catch (const std::exception &e) {
        d->lastError = QStringLiteral("ORT 加载异常: %1").arg(QString::fromUtf8(e.what()));
        d->session.reset();
        return false;
    }
}

bool OrtInferenceEngine::isLoaded() const
{
    return d && d->loaded;
}

std::vector<std::vector<float>> OrtInferenceEngine::run(const std::vector<float> &input,
                                                        const std::vector<int64_t> &inputShape,
                                                        std::vector<std::vector<int64_t>> *outShapes)
{
    std::vector<std::vector<float>> outputs;
    if (!isLoaded()) {
        d->lastError = QStringLiteral("模型未加载");
        return outputs;
    }
    if (input.empty() || inputShape.empty()) {
        d->lastError = QStringLiteral("输入张量为空");
        return outputs;
    }

    try {
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float *>(input.data()), input.size(),
            inputShape.data(), inputShape.size());

        // 持久保存 QByteArray，保证 Run 调用期间 const char* 缓冲存活，避免悬垂指针。
        std::vector<QByteArray> inUtf8;
        std::vector<const char *> inNames;
        inUtf8.reserve(d->inputNames.size());
        for (const QString &n : d->inputNames) {
            inUtf8.push_back(n.toUtf8());
            inNames.push_back(inUtf8.back().constData());
        }
        std::vector<QByteArray> outUtf8;
        std::vector<const char *> outNames;
        outUtf8.reserve(d->outputNames.size());
        for (const QString &n : d->outputNames) {
            outUtf8.push_back(n.toUtf8());
            outNames.push_back(outUtf8.back().constData());
        }

        auto outputTensors = d->session->Run(Ort::RunOptions{nullptr}, inNames.data(),
                                             &inputTensor, 1, outNames.data(), outNames.size());

        for (auto &tensor : outputTensors) {
            const auto &typeInfo = tensor.GetTensorTypeAndShapeInfo();
            const auto shape = typeInfo.GetShape();
            const size_t total = typeInfo.GetElementCount();
            const float *data = tensor.GetTensorData<float>();
            outputs.emplace_back(data, data + total);
            if (outShapes)
                outShapes->emplace_back(shape.begin(), shape.end());
        }
        d->lastError.clear();
    } catch (const Ort::Exception &e) {
        d->lastError = QStringLiteral("ORT 推理失败: %1").arg(QString::fromUtf8(e.what()));
    } catch (const std::exception &e) {
        d->lastError = QStringLiteral("ORT 推理异常: %1").arg(QString::fromUtf8(e.what()));
    }
    return outputs;
}

QStringList OrtInferenceEngine::inputNames() const
{
    return d ? d->inputNames : QStringList();
}

QStringList OrtInferenceEngine::outputNames() const
{
    return d ? d->outputNames : QStringList();
}

QString OrtInferenceEngine::lastError() const
{
    return d ? d->lastError : QString();
}

D_TABLERECOGNIZER_END_NAMESPACE
