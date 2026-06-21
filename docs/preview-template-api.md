# 模板打印预览接口规范

本文档面向 Web 前端，说明在真正调用打印前，如何向 sleekpr 本地客户端请求模板预览图。

## 基本说明

- 服务地址：`http://127.0.0.1:37122`
- 预览接口：`POST /preview/template`
- 请求格式：`Content-Type: application/json`
- 返回格式：`application/json; charset=utf-8`
- 预览结果：PNG 图片的 Base64 字符串
- 预览不会触发真实打印。
- 预览由本地 Qt 原生渲染链路生成，Web 端只负责展示图片，不要在浏览器里重新排版模板。
- 预览接口和 `/print/template` 复用同一套模板解析、字段占位符、数组网格、二维码、纸张规格、DPI 和设备 profile 逻辑。

## 调用前准备

1. 确认本地客户端已启动：

```http
GET http://127.0.0.1:37122/health
```

2. 确认 Web 当前页面 Origin 已加入客户端设置里的 `allowedOrigins`。

例如前端页面是：

```text
https://manager.example.com
```

则 `settings.allowedOrigins` 中需要包含：

```json
[
  "https://manager.example.com"
]
```

3. 确认前端知道要使用哪个模板。

可以通过模板库接口查看：

```http
GET http://127.0.0.1:37122/templates
```

优先使用模板的 `templateKey`，例如 `default`。如果不传模板键，服务端会按 `default` 查找模板。

## 单条预览

### 请求

```http
POST http://127.0.0.1:37122/preview/template
Content-Type: application/json
```

```json
{
  "requestId": "preview-1",
  "templateKey": "default",
  "printerName": "标签打印机",
  "fieldPresetId": "default-fields",
  "values": {
    "salesCode": "606178PD35",
    "sale_attach_text": "附加: ¥140.00",
    "identifierCode": "1210005822",
    "productName": "足金串搭项链",
    "address": "水贝金座一楼1111民族工匠"
  }
}
```

### 请求字段

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `requestId` | string | 否 | 前端请求编号。返回时原样带回；不传时服务端自动生成。 |
| `templateKey` | string | 否 | 模板键。建议传；不传时默认使用 `default`。 |
| `templateId` | string | 否 | 模板 ID。和 `templateKey` 二选一即可。 |
| `printerName` | string | 否 | 打印机名称。预览不会打印，但会用于选择对应设备 profile、DPI、缩放和偏移。 |
| `fieldPresetId` | string | 否 | 字段预设 ID。也兼容 `presetId`。 |
| `values` | object | 否 | 本次预览数据。模板里的 `${字段名}` 会用这里的实际值替换。 |

### 成功响应

```json
{
  "success": true,
  "code": "OK",
  "message": "",
  "data": {
    "requestId": "preview-1",
    "totalItems": 1,
    "totalPages": 1,
    "paper": {
      "widthMm": 80,
      "heightMm": 30,
      "dpi": 203
    },
    "pages": [
      {
        "pageNumber": 1,
        "jobPageNumber": 1,
        "itemIndex": 0,
        "requestId": "preview-1",
        "templateKey": "default",
        "contentType": "image/png",
        "imageBase64": "iVBORw0KGgo...",
        "paper": {
          "widthMm": 80,
          "heightMm": 30,
          "dpi": 203
        }
      }
    ]
  }
}
```

### 响应字段

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `data.requestId` | string | 本次预览请求编号。 |
| `data.totalItems` | number | 本次预览的数据条数。单条预览为 `1`。 |
| `data.totalPages` | number | 返回的预览图片总页数。 |
| `data.paper` | object | 第一张预览使用的纸张信息。 |
| `data.pages` | array | 预览页列表。 |
| `pageNumber` | number | 全局页码，从 `1` 开始。批量预览时连续递增。 |
| `jobPageNumber` | number | 单条任务内部页码，从 `1` 开始。 |
| `itemIndex` | number | 批量数据下标，从 `0` 开始。单条预览为 `0`。 |
| `requestId` | string | 当前页对应的数据请求编号。 |
| `templateKey` | string | 当前页使用的模板键。 |
| `contentType` | string | 当前固定为 `image/png`。 |
| `imageBase64` | string | PNG 图片 Base64，不包含 `data:image/png;base64,` 前缀。 |
| `paper` | object | 当前页的纸张信息。 |

## 批量预览

批量预览使用同一个接口。顶层字段作为公共配置，每个 `items` 项可以只传自己的 `values`，也可以覆盖 `templateKey`、`printerName`、`fieldPresetId`。

### 请求

```http
POST http://127.0.0.1:37122/preview/template
Content-Type: application/json
```

```json
{
  "requestId": "batch-preview-1",
  "templateKey": "default",
  "printerName": "标签打印机",
  "items": [
    {
      "requestId": "item-1",
      "values": {
        "salesCode": "606178PD35",
        "identifierCode": "1210005822",
        "productName": "足金串搭项链"
      }
    },
    {
      "requestId": "item-2",
      "values": {
        "salesCode": "606178PD36",
        "identifierCode": "1210005823",
        "productName": "足金戒指"
      }
    }
  ]
}
```

也兼容把 `items` 写成 `jobs`：

```json
{
  "templateKey": "default",
  "jobs": [
    {
      "requestId": "item-1",
      "values": {
        "productName": "足金项链"
      }
    }
  ]
}
```

### 批量项字段

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `requestId` | string | 否 | 当前数据项编号。不传时服务端按批次编号自动生成。 |
| `templateKey` | string | 否 | 当前数据项单独指定模板；不传则继承顶层 `templateKey`。 |
| `templateId` | string | 否 | 当前数据项单独指定模板 ID。 |
| `printerName` | string | 否 | 当前数据项单独指定打印机 profile；不传则继承顶层 `printerName`。 |
| `fieldPresetId` | string | 否 | 当前数据项单独指定字段预设；不传则继承顶层 `fieldPresetId`。 |
| `values` | object | 否 | 当前数据项的实际字段值。 |

### 成功响应

```json
{
  "success": true,
  "code": "OK",
  "message": "",
  "data": {
    "requestId": "batch-preview-1",
    "totalItems": 2,
    "totalPages": 2,
    "paper": {
      "widthMm": 80,
      "heightMm": 30,
      "dpi": 203
    },
    "pages": [
      {
        "pageNumber": 1,
        "jobPageNumber": 1,
        "itemIndex": 0,
        "requestId": "item-1",
        "templateKey": "default",
        "contentType": "image/png",
        "imageBase64": "iVBORw0KGgo..."
      },
      {
        "pageNumber": 2,
        "jobPageNumber": 1,
        "itemIndex": 1,
        "requestId": "item-2",
        "templateKey": "default",
        "contentType": "image/png",
        "imageBase64": "iVBORw0KGgo..."
      }
    ]
  }
}
```

## 数组数据

`values` 可以包含数组。模板设计器中的数组网格元素会按元素配置的 `dataPath` 读取数组。

示例：

```json
{
  "requestId": "preview-array-1",
  "templateKey": "default",
  "values": {
    "salesCode": "606178PD35",
    "finishedProductPartVO": [
      {
        "categoryName": "素金KA",
        "partWeight": "2.99"
      },
      {
        "categoryName": "素金PC",
        "partWeight": "4.13"
      }
    ]
  }
}
```

如果模板中的数组网格 `dataPath` 配置为 `finishedProductPartVO`，就会读取上面的数组进行渲染。

## 前端 JS 调用示例

项目提供了 `web/sleekpr-print-client.js`。

### 单条预览

```js
import {
  createSleekprPrintClient,
  previewPageDataUrl
} from "./sleekpr-print-client.js";

const client = createSleekprPrintClient({
  baseUrl: "http://127.0.0.1:37122"
});

const response = await client.previewTemplate({
  requestId: "preview-1",
  templateKey: "default",
  printerName: "标签打印机",
  values: {
    productName: "足金串搭项链",
    identifierCode: "1210005822"
  }
});

const firstPage = response.data.pages[0];
document.querySelector("#preview").src = previewPageDataUrl(firstPage);
```

### 批量预览

```js
const response = await client.previewTemplatesBatch({
  requestId: "batch-preview-1",
  templateKey: "default",
  printerName: "标签打印机",
  jobs: [
    {
      requestId: "item-1",
      values: {
        productName: "足金项链",
        identifierCode: "1210005822"
      }
    },
    {
      requestId: "item-2",
      values: {
        productName: "足金戒指",
        identifierCode: "1210005823"
      }
    }
  ]
});

const container = document.querySelector("#preview-list");
container.innerHTML = "";

for (const page of response.data.pages) {
  const img = document.createElement("img");
  img.src = previewPageDataUrl(page);
  img.dataset.pageNumber = String(page.pageNumber);
  img.dataset.itemIndex = String(page.itemIndex);
  container.appendChild(img);
}
```

### 预览后确认打印

用户确认预览无误后，再调用真实打印接口：

```js
await client.printTemplate({
  requestId: "print-1",
  templateKey: "default",
  printerName: "标签打印机",
  executePrint: true,
  values: {
    productName: "足金串搭项链",
    identifierCode: "1210005822"
  }
});
```

## 错误响应

错误响应统一为：

```json
{
  "success": false,
  "code": "BAD_REQUEST",
  "message": "错误说明",
  "data": {}
}
```

常见错误码：

| HTTP 状态 | code | 说明 |
| --- | --- | --- |
| `403` | `FORBIDDEN_ORIGIN` | 当前网页 Origin 未加入 `allowedOrigins`。 |
| `400` | `BAD_REQUEST` | JSON 格式错误、`values` 不是对象、`items` 不是数组或批量项格式错误。 |
| `400` | `TEMPLATE_NOT_FOUND` | 未找到指定模板。 |
| `400` | `FIELD_PRESET_NOT_FOUND` | 未找到指定字段预设。 |
| `400` | `TEMPLATE_VALIDATION_FAILED` | 模板校验失败，例如字段缺失、元素越界、二维码内容异常等。 |

前端建议优先根据 `success` 和 `code` 判断业务结果，不要只依赖 HTTP 状态码。

## 前端展示建议

- `imageBase64` 不包含 data URL 前缀，展示前需要拼接：

```js
const src = `data:${page.contentType};base64,${page.imageBase64}`;
```

- 批量预览建议使用缩略图列表加大图区域。缩略图按 `pageNumber` 排序展示。
- 如果要关联业务数据，使用 `itemIndex` 或 `requestId` 映射回原始 `jobs/items`。
- 预览图是服务端按纸张 DPI 生成的 PNG，页面展示时可以按容器缩放；真实打印仍以服务端打印链路为准。
- 不建议把完整 `imageBase64` 写入业务日志，避免日志体积过大。
