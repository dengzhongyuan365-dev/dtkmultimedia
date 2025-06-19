#ifndef DOCR_C_API_H
#define DOCR_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

// 导出符号宏定义 - 改进版本
#ifdef WIN32
    #ifdef BUILDING_DTKOCR
        #define DTKOCR_EXPORT __declspec(dllexport)
    #else
        #define DTKOCR_EXPORT __declspec(dllimport)
    #endif
#else
    #ifdef BUILDING_DTKOCR
        // 构建库时，显式导出符号
        #define DTKOCR_EXPORT __attribute__((visibility("default")))
    #else
        // 使用库时，声明为外部符号
        #define DTKOCR_EXPORT __attribute__((visibility("default")))
    #endif
#endif

// API版本信息
#define DTKOCR_API_VERSION_MAJOR 1
#define DTKOCR_API_VERSION_MINOR 0
#define DTKOCR_API_VERSION_PATCH 0

// 确保C API的ABI稳定性
#ifndef DTKOCR_NO_DEPRECATED
    #define DTKOCR_DEPRECATED __attribute__((deprecated))
#else
    #define DTKOCR_DEPRECATED
#endif

// OCR句柄类型
typedef void* OCRHandle;

// 硬件ID枚举
typedef enum {
    HARDWARE_CPU_ANY = 0,
    HARDWARE_GPU_VULKAN = 101
} HardwareType;

// 文本框结构体
typedef struct {
    float points[8]; // 4个点的x,y坐标
    float angle;
} TextBoxC;

// 文本框列表结构体
typedef struct {
    TextBoxC* boxes;
    int count;
} TextBoxListC;

// 基础API
DTKOCR_EXPORT OCRHandle ocr_create(void);
DTKOCR_EXPORT void ocr_destroy(OCRHandle handle);
DTKOCR_EXPORT int ocr_load_default_plugin(OCRHandle handle);
DTKOCR_EXPORT int ocr_plugin_ready(OCRHandle handle);

// 硬件配置API
DTKOCR_EXPORT int ocr_set_hardware(OCRHandle handle, HardwareType type, int device_id);
DTKOCR_EXPORT int ocr_set_max_threads(OCRHandle handle, int count);

// 语言设置API
DTKOCR_EXPORT int ocr_set_language(OCRHandle handle, const char* language);

// 图像设置API
DTKOCR_EXPORT int ocr_set_image_file(OCRHandle handle, const char* file_path);
DTKOCR_EXPORT int ocr_set_image_data(OCRHandle handle, const unsigned char* data, 
                                    int width, int height, int channels);

// 识别API
DTKOCR_EXPORT int ocr_analyze(OCRHandle handle);
DTKOCR_EXPORT int ocr_break_analyze(OCRHandle handle);
DTKOCR_EXPORT int ocr_is_running(OCRHandle handle);

// 结果获取API
DTKOCR_EXPORT const char* ocr_get_simple_result(OCRHandle handle);
DTKOCR_EXPORT TextBoxListC* ocr_get_text_boxes(OCRHandle handle);
DTKOCR_EXPORT void ocr_free_text_boxes(TextBoxListC* boxes);

// 版本信息
DTKOCR_EXPORT const char* ocr_get_version(void);

// API版本查询函数
DTKOCR_EXPORT int ocr_get_api_version_major(void);
DTKOCR_EXPORT int ocr_get_api_version_minor(void);
DTKOCR_EXPORT int ocr_get_api_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif // DOCR_C_API_H 