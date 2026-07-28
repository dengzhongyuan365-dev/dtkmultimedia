# SLANet_plus ONNX 模型占位

本目录用于随包分发表格结构识别模型 `SLANet_plus.onnx`（约 7.4MB，Apache-2.0）。

## 模型来源与法务（Phase 0 前置阻断项）

- 模型按 **Apache-2.0** 标注（非 futz12 仓库的 MIT）。
- 需在 Phase 0 核验模型卡 license 与 `_plus` 数据 provenance，确认标注合规后方可入库。

## 放置方式

将确认合规的模型文件放置为：

```
src/tablerecognizer/models/SLANet_plus.onnx
```

Debug 构建通过 `-DTABLEREC_MODEL_DIR` 指向本源码目录；Release 构建安装到
`/usr/share/libdtk6tablerecognizer/models/`。

> 当前为占位目录，模型文件未入库（待 Phase 0 法务 spot-check 通过后补入）。
