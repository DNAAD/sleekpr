import assert from "node:assert/strict";
import { Buffer } from "node:buffer";
import { readFile } from "node:fs/promises";
import test from "node:test";

async function loadPrintClientModule() {
  const source = await readFile(new URL("../../web/sleekpr-print-client.js", import.meta.url), "utf8");
  const moduleUrl = `data:text/javascript;base64,${Buffer.from(source).toString("base64")}`;
  return import(moduleUrl);
}

function jsonResponse(status, body) {
  return {
    ok: status >= 200 && status < 300,
    status,
    statusText: status === 200 ? "OK" : "ERROR",
    headers: {
      get(name) {
        return name.toLowerCase() === "content-type" ? "application/json" : "";
      },
    },
    async text() {
      return JSON.stringify(body);
    },
  };
}

test("printTemplate 使用本地模板打印接口和服务端字段名", async () => {
  const { SleekprPrintClient } = await loadPrintClientModule();
  const requests = [];
  const client = new SleekprPrintClient({
    baseUrl: "http://127.0.0.1:37122",
    fetcher: async (url, options) => {
      requests.push({ url, options });
      return jsonResponse(200, {
        success: true,
        data: {
          requestId: "req-1",
          accepted: 1,
          printed: 1,
          failed: 0,
          executePrint: true,
        },
      });
    },
  });

  const result = await client.printTemplate({
    requestId: "req-1",
    templateKey: "default",
    fieldPresetId: "preset-1",
    printerName: "标签打印机",
    values: {
      product_name: "足金串搭项链",
    },
  });

  assert.equal(result.success, true);
  assert.equal(requests.length, 1);
  assert.equal(requests[0].url, "http://127.0.0.1:37122/print/template");
  assert.equal(requests[0].options.method, "POST");
  assert.equal(requests[0].options.headers["Content-Type"], "application/json");
  assert.deepEqual(JSON.parse(requests[0].options.body), {
    requestId: "req-1",
    templateKey: "default",
    fieldPresetId: "preset-1",
    printerName: "标签打印机",
    executePrint: true,
    values: {
      product_name: "足金串搭项链",
    },
  });
});

test("HTTP 或业务失败时抛出带状态码和错误码的异常", async () => {
  const { SleekprPrintClient, SleekprPrintError } = await loadPrintClientModule();
  const client = new SleekprPrintClient({
    fetcher: async () =>
      jsonResponse(400, {
        success: false,
        code: "BAD_REQUEST",
        message: "字段值 values 必须是对象。",
      }),
  });

  await assert.rejects(
    () => client.printTemplate({ templateKey: "default", values: [] }),
    (error) => {
      assert.ok(error instanceof SleekprPrintError);
      assert.equal(error.status, 400);
      assert.equal(error.code, "BAD_REQUEST");
      assert.match(error.message, /values/);
      return true;
    },
  );
});

test("printTemplatesBatch 按队列批量调用模板打印并汇总失败项", async () => {
  const { SleekprPrintClient } = await loadPrintClientModule();
  const requests = [];
  const client = new SleekprPrintClient({
    fetcher: async (url, options) => {
      requests.push({ url, options });
      if (requests.length === 2) {
        return jsonResponse(400, {
          success: false,
          code: "BAD_REQUEST",
          message: "缺少商品名称",
        });
      }

      return jsonResponse(200, {
        success: true,
        data: {
          accepted: 1,
          printed: 1,
          failed: 0,
        },
      });
    },
  });

  const result = await client.printTemplatesBatch({
    templateKey: "default",
    printerName: "标签打印机",
    jobs: [
      { requestId: "batch-1", values: { product_name: "足金串搭项链" } },
      { requestId: "batch-2", values: {} },
      { requestId: "batch-3", values: { product_name: "足金戒指" } },
    ],
  });

  assert.equal(requests.length, 3);
  assert.deepEqual(JSON.parse(requests[0].options.body), {
    requestId: "batch-1",
    templateKey: "default",
    printerName: "标签打印机",
    executePrint: true,
    values: { product_name: "足金串搭项链" },
  });
  assert.deepEqual(JSON.parse(requests[2].options.body), {
    requestId: "batch-3",
    templateKey: "default",
    printerName: "标签打印机",
    executePrint: true,
    values: { product_name: "足金戒指" },
  });
  assert.equal(result.total, 3);
  assert.equal(result.successful, 2);
  assert.equal(result.failed, 1);
  assert.equal(result.success, false);
  assert.equal(result.results[1].success, false);
  assert.equal(result.results[1].error.code, "BAD_REQUEST");
});
