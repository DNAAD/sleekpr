/**
 * sleekpr 本地打印服务浏览器端调用工具。
 *
 * 使用前请先在 sleekpr 设置页的“本地接口安全”中，把当前网页 Origin 加入 allowedOrigins。
 * 例如本地开发填 http://localhost:5173，生产管理端填 https://manager.example.com。
 */

const DEFAULT_BASE_URL = "http://127.0.0.1:37122";
const DEFAULT_TIMEOUT_MS = 10000;

export class SleekprPrintError extends Error {
  constructor(message, options = {}) {
    super(message);
    this.name = "SleekprPrintError";
    this.status = options.status ?? 0;
    this.code = options.code ?? "PRINT_SERVICE_ERROR";
    this.payload = options.payload ?? null;
  }
}

export class SleekprPrintClient {
  constructor(options = {}) {
    this.baseUrl = normalizeBaseUrl(options.baseUrl ?? DEFAULT_BASE_URL);
    this.timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;
    this.fetcher = options.fetcher ?? globalThis.fetch?.bind(globalThis);
    this.token = options.token ?? "";
    this.authorization = options.authorization ?? "";

    if (typeof this.fetcher !== "function") {
      throw new TypeError("当前运行环境缺少 fetch，请传入 fetcher 或使用现代浏览器。");
    }
  }

  health() {
    return this.request("/health");
  }

  getSettings() {
    return this.request("/settings");
  }

  /**
   * 保存完整 settings.json 结构，不是局部 patch。
   * 如果只是配置 CORS，推荐先 getSettings()，修改 allowedOrigins 后再 saveSettings()。
   */
  saveSettings(settings) {
    return this.request("/settings", {
      method: "POST",
      body: settings,
    });
  }

  getPrinters() {
    return this.request("/printers");
  }

  getTemplates() {
    return this.request("/templates");
  }

  getTemplate(templateId) {
    return this.request(`/templates/${encodeURIComponent(templateId)}`);
  }

  saveTemplate(templateDocument) {
    return this.request("/templates", {
      method: "POST",
      body: templateDocument,
    });
  }

  deleteTemplate(templateId) {
    return this.request(`/templates/${encodeURIComponent(templateId)}`, {
      method: "DELETE",
    });
  }

  getPaperSpecs() {
    return this.request("/paper-specs");
  }

  getFieldPresets() {
    return this.request("/field-presets");
  }

  printTag(options = {}) {
    const items = options.items ?? [];
    if (!Array.isArray(items) || items.length === 0) {
      throw new TypeError("printTag 需要传入至少一条 items 标签数据。");
    }

    return this.request("/print/tag", {
      method: "POST",
      body: compactObject({
        requestId: options.requestId,
        printerName: options.printerName,
        executePrint: options.executePrint ?? true,
        items,
      }),
    });
  }

  printTemplate(options = {}) {
    return this.request("/print/template", {
      method: "POST",
      body: compactObject({
        requestId: options.requestId,
        templateKey: options.templateKey,
        templateId: options.templateId,
        fieldPresetId: options.fieldPresetId,
        printerName: options.printerName,
        executePrint: options.executePrint ?? true,
        values: options.values ?? {},
      }),
    });
  }

  previewTemplate(options = {}) {
    const body = compactObject({
      requestId: options.requestId,
      templateKey: options.templateKey,
      templateId: options.templateId,
      fieldPresetId: options.fieldPresetId,
      printerName: options.printerName,
    });

    if (Array.isArray(options.items)) {
      body.items = options.items;
    } else {
      body.values = options.values ?? {};
    }

    return this.request("/preview/template", {
      method: "POST",
      body,
    });
  }

  async printTemplatesBatch(options = {}) {
    const jobs = options.jobs ?? [];
    if (!Array.isArray(jobs) || jobs.length === 0) {
      throw new TypeError("printTemplatesBatch 需要传入至少一条 jobs 模板打印任务。");
    }

    const continueOnError = options.continueOnError ?? true;
    const commonRequest = compactObject({
      templateKey: options.templateKey,
      templateId: options.templateId,
      fieldPresetId: options.fieldPresetId,
      printerName: options.printerName,
      executePrint: options.executePrint ?? true,
    });
    const results = [];

    for (let index = 0; index < jobs.length; index += 1) {
      const request = compactObject({
        ...commonRequest,
        ...(jobs[index] ?? {}),
        values: jobs[index]?.values ?? {},
      });

      try {
        const response = await this.printTemplate(request);
        results.push({
          index,
          success: true,
          request,
          response,
        });
      } catch (error) {
        results.push({
          index,
          success: false,
          request,
          error: batchErrorPayload(error),
        });

        if (!continueOnError) {
          const batchError = new SleekprPrintError(`批量模板打印在第 ${index + 1} 条失败。`, {
            status: error?.status,
            code: error?.code ?? "BATCH_PRINT_FAILED",
            payload: {
              index,
              results,
            },
          });
          throw batchError;
        }
      }
    }

    const failed = results.filter((item) => !item.success).length;
    return {
      success: failed === 0,
      total: jobs.length,
      successful: jobs.length - failed,
      failed,
      results,
    };
  }

  previewTemplatesBatch(options = {}) {
    const jobs = options.jobs ?? options.items ?? [];
    if (!Array.isArray(jobs) || jobs.length === 0) {
      throw new TypeError("previewTemplatesBatch 需要传入至少一条 jobs 模板预览任务。");
    }

    return this.request("/preview/template", {
      method: "POST",
      body: compactObject({
        requestId: options.requestId,
        templateKey: options.templateKey,
        templateId: options.templateId,
        fieldPresetId: options.fieldPresetId,
        printerName: options.printerName,
        items: jobs.map((job) =>
          compactObject({
            ...(job ?? {}),
            values: job?.values ?? {},
          }),
        ),
      }),
    });
  }

  async request(path, options = {}) {
    const method = (options.method ?? "GET").toUpperCase();
    const headers = {
      Accept: "application/json",
      ...(options.headers ?? {}),
    };
    const requestOptions = {
      method,
      headers,
    };

    if (this.token) {
      headers["X-Sleekpr-Token"] = this.token;
    }
    if (this.authorization) {
      headers.Authorization = this.authorization;
    }
    if (options.body !== undefined) {
      headers["Content-Type"] = headers["Content-Type"] ?? "application/json";
      requestOptions.body = JSON.stringify(options.body);
    }

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), this.timeoutMs);
    requestOptions.signal = controller.signal;

    try {
      const response = await this.fetcher(buildUrl(this.baseUrl, path), requestOptions);
      const payload = await parseJsonResponse(response);
      if (!response.ok || payload?.success === false) {
        throw createPrintError(response, payload);
      }
      return payload;
    } catch (error) {
      if (error instanceof SleekprPrintError) {
        throw error;
      }
      if (error?.name === "AbortError") {
        throw new SleekprPrintError(`调用 sleekpr 本地打印服务超时：${this.timeoutMs}ms`, {
          code: "REQUEST_TIMEOUT",
        });
      }
      throw new SleekprPrintError(error?.message ?? "调用 sleekpr 本地打印服务失败", {
        code: "NETWORK_ERROR",
        payload: error,
      });
    } finally {
      clearTimeout(timeoutId);
    }
  }
}

export function createSleekprPrintClient(options = {}) {
  return new SleekprPrintClient(options);
}

export function previewPageDataUrl(page) {
  const contentType = page?.contentType || "image/png";
  return `data:${contentType};base64,${page?.imageBase64 ?? ""}`;
}

function normalizeBaseUrl(baseUrl) {
  return String(baseUrl).replace(/\/+$/, "");
}

function buildUrl(baseUrl, path) {
  const normalizedPath = String(path).startsWith("/") ? String(path) : `/${path}`;
  return `${baseUrl}${normalizedPath}`;
}

function compactObject(value) {
  return Object.fromEntries(Object.entries(value).filter(([, item]) => item !== undefined));
}

function batchErrorPayload(error) {
  return {
    name: error?.name ?? "Error",
    message: error?.message ?? "批量打印任务失败",
    status: error?.status ?? 0,
    code: error?.code ?? "PRINT_JOB_FAILED",
    payload: error?.payload ?? null,
  };
}

async function parseJsonResponse(response) {
  const text = await response.text();
  if (!text) {
    return null;
  }

  try {
    return JSON.parse(text);
  } catch (error) {
    throw new SleekprPrintError("sleekpr 本地打印服务返回了非 JSON 内容。", {
      status: response.status,
      code: "INVALID_JSON_RESPONSE",
      payload: {
        text,
        parseError: error?.message,
      },
    });
  }
}

function createPrintError(response, payload) {
  const message = payload?.message ?? response.statusText ?? "调用 sleekpr 本地打印服务失败";
  return new SleekprPrintError(message, {
    status: response.status,
    code: payload?.code ?? `HTTP_${response.status}`,
    payload,
  });
}

if (typeof window !== "undefined") {
  window.SleekprPrintClient = SleekprPrintClient;
  window.SleekprPrintError = SleekprPrintError;
  window.createSleekprPrintClient = createSleekprPrintClient;
  window.previewPageDataUrl = previewPageDataUrl;
}
