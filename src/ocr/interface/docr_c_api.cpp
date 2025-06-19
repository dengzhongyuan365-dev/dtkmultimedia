#include "docr_c_api.h"
#include "docr.h"
#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>
#include <cstring>
#include <memory>

DOCR_USE_NAMESPACE

// 内部结构体，用于管理OCR实例和相关资源
struct OCRInstance {
    std::unique_ptr<DOcr> ocr;
    QString lastResult;
    QByteArray lastResultBytes;  // 修复：存储UTF-8字节数组
    std::unique_ptr<TextBoxListC> lastTextBoxes;
    
    OCRInstance() : ocr(std::make_unique<DOcr>()) {}
    
    ~OCRInstance() {
        if (lastTextBoxes && lastTextBoxes->boxes) {
            delete[] lastTextBoxes->boxes;
        }
    }
};

// 辅助函数：将QString转换为C字符串
static const char* qstring_to_cstr(const QString& str, QString& storage) {
    storage = str;
    return storage.toUtf8().constData();
}

// 基础API实现
extern "C" {

DTKOCR_EXPORT OCRHandle ocr_create(void) {
    try {
        return new OCRInstance();
    } catch (...) {
        return nullptr;
    }
}

DTKOCR_EXPORT void ocr_destroy(OCRHandle handle) {
    if (handle) {
        delete static_cast<OCRInstance*>(handle);
    }
}

DTKOCR_EXPORT int ocr_load_default_plugin(OCRHandle handle) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->loadDefaultPlugin() ? 1 : 0;
}

DTKOCR_EXPORT int ocr_plugin_ready(OCRHandle handle) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->pluginReady() ? 1 : 0;
}

// 硬件配置API实现
DTKOCR_EXPORT int ocr_set_hardware(OCRHandle handle, HardwareType type, int device_id) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    
    HardwareID hw_id;
    switch (type) {
        case HARDWARE_CPU_ANY:
            hw_id = CPU_Any;
            break;
        case HARDWARE_GPU_VULKAN:
            hw_id = GPU_Vulkan;
            break;
        default:
            return 0;
    }
    
    QList<QPair<HardwareID, int>> hardware = {{hw_id, device_id}};
    return instance->ocr->setUseHardware(hardware) ? 1 : 0;
}

DTKOCR_EXPORT int ocr_set_max_threads(OCRHandle handle, int count) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->setUseMaxThreadsCount(count) ? 1 : 0;
}

// 语言设置API实现
DTKOCR_EXPORT int ocr_set_language(OCRHandle handle, const char* language) {
    if (!handle || !language) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->setLanguage(QString::fromUtf8(language)) ? 1 : 0;
}

// 图像设置API实现
DTKOCR_EXPORT int ocr_set_image_file(OCRHandle handle, const char* file_path) {
    if (!handle || !file_path) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->setImageFile(QString::fromUtf8(file_path)) ? 1 : 0;
}

DTKOCR_EXPORT int ocr_set_image_data(OCRHandle handle, const unsigned char* data, 
                                    int width, int height, int channels) {
    if (!handle || !data || width <= 0 || height <= 0) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    
    QImage::Format format;
    int bytesPerLine;
    switch (channels) {
        case 1:
            format = QImage::Format_Grayscale8;
            bytesPerLine = width;
            break;
        case 3:
            format = QImage::Format_RGB888;
            bytesPerLine = width * 3;
            break;
        case 4:
            format = QImage::Format_RGBA8888;
            bytesPerLine = width * 4;
            break;
        default:
            return 0;
    }
    
    // 创建QImage并复制数据，确保数据生命周期安全
    QImage image(width, height, format);
    if (image.isNull()) {
        return 0;
    }
    
    // 复制数据到QImage的内部缓冲区
    const int dataSize = height * bytesPerLine;
    std::memcpy(image.bits(), data, dataSize);
    
    return instance->ocr->setImage(image) ? 1 : 0;
}

// 识别API实现
DTKOCR_EXPORT int ocr_analyze(OCRHandle handle) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->analyze() ? 1 : 0;
}

DTKOCR_EXPORT int ocr_break_analyze(OCRHandle handle) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->breakAnalyze() ? 1 : 0;
}

DTKOCR_EXPORT int ocr_is_running(OCRHandle handle) {
    if (!handle) return 0;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    return instance->ocr->isRunning() ? 1 : 0;
}

// 结果获取API实现 - 修复字符串生命周期问题
DTKOCR_EXPORT const char* ocr_get_simple_result(OCRHandle handle) {
    if (!handle) return nullptr;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    instance->lastResult = instance->ocr->simpleResult();
    
    // 修复：将QString转换为QByteArray并保存，确保指针有效
    instance->lastResultBytes = instance->lastResult.toUtf8();
    return instance->lastResultBytes.constData();
}

DTKOCR_EXPORT TextBoxListC* ocr_get_text_boxes(OCRHandle handle) {
    if (!handle) return nullptr;
    
    auto* instance = static_cast<OCRInstance*>(handle);
    auto textBoxes = instance->ocr->textBoxes();
    
    // 释放之前的内存
    if (instance->lastTextBoxes && instance->lastTextBoxes->boxes) {
        delete[] instance->lastTextBoxes->boxes;
    }
    
    instance->lastTextBoxes = std::make_unique<TextBoxListC>();
    instance->lastTextBoxes->count = textBoxes.size();
    
    if (textBoxes.empty()) {
        instance->lastTextBoxes->boxes = nullptr;
        return instance->lastTextBoxes.get();
    }
    
    instance->lastTextBoxes->boxes = new TextBoxC[textBoxes.size()];
    
    for (int i = 0; i < textBoxes.size(); ++i) {
        const auto& box = textBoxes[i];
        auto& cBox = instance->lastTextBoxes->boxes[i];
        
        cBox.angle = box.angle;
        
        // 复制点坐标（最多4个点）
        int pointCount = qMin(4, box.points.size());
        for (int j = 0; j < pointCount; ++j) {
            cBox.points[j * 2] = box.points[j].x();
            cBox.points[j * 2 + 1] = box.points[j].y();
        }
        
        // 填充剩余点为0
        for (int j = pointCount; j < 4; ++j) {
            cBox.points[j * 2] = 0.0f;
            cBox.points[j * 2 + 1] = 0.0f;
        }
    }
    
    return instance->lastTextBoxes.get();
}

DTKOCR_EXPORT void ocr_free_text_boxes(TextBoxListC* boxes) {
    // 由于内存由OCRInstance管理，这里不需要释放
    // 仅为API完整性保留
    (void)boxes; // 避免未使用参数警告
}

// 版本信息 - 改进版本
DTKOCR_EXPORT const char* ocr_get_version(void) {
    static const char* version = VERSION; // 从CMake获取版本
    return version;
}

// 新增：API版本查询函数
DTKOCR_EXPORT int ocr_get_api_version_major(void) {
    return DTKOCR_API_VERSION_MAJOR;
}

DTKOCR_EXPORT int ocr_get_api_version_minor(void) {
    return DTKOCR_API_VERSION_MINOR;
}

DTKOCR_EXPORT int ocr_get_api_version_patch(void) {
    return DTKOCR_API_VERSION_PATCH;
}

} // extern "C" 