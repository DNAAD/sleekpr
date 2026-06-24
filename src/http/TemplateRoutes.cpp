#include "sleekpr/http/TemplateRoutes.h"

#include "sleekpr/core/templates/FieldPresetJson.h"
#include "sleekpr/core/templates/PaperSpecJson.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace sleekpr::http {

using sleekpr::core::FieldPreset;
using sleekpr::core::FieldPresetJson;
using sleekpr::core::FieldPresetStore;
using sleekpr::core::PaperSpec;
using sleekpr::core::PaperSpecJson;
using sleekpr::core::PaperSpecStore;
using sleekpr::core::TemplateDocument;
using sleekpr::core::TemplateDocumentJson;
using sleekpr::core::TemplateLibraryStore;

namespace {

QJsonObject templateSummaryJson(const TemplateDocument& document)
{
    return QJsonObject{
        {"id", document.id},
        {"name", document.name},
        {"templateKey", document.templateKey},
        {"category", document.category},
        {"paperSpecId", document.paperSpecId},
        {"schemaVersion", document.schemaVersion},
        {"activeVersionId", document.activeVersionId},
    };
}

QJsonArray templateSummaryListJson(const TemplateLibraryStore& store)
{
    QJsonArray templates;
    for (const auto& templateId : store.templateIds()) {
        QString errorMessage;
        const auto document = store.loadTemplate(templateId, &errorMessage);
        if (!document.has_value()) {
            continue;
        }
        templates.append(templateSummaryJson(document.value()));
    }
    return templates;
}

std::optional<QJsonArray> paperSpecListJson(const PaperSpecStore& store, QString* errorMessage)
{
    QJsonArray paperSpecs;
    QString storeError;
    const auto specs = store.paperSpecs(&storeError);
    if (!storeError.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        return std::nullopt;
    }

    for (const auto& spec : specs) {
        paperSpecs.append(PaperSpecJson::toJson(spec));
    }
    return paperSpecs;
}

std::optional<QJsonArray> fieldPresetListJson(const FieldPresetStore& store, QString* errorMessage)
{
    QJsonArray presets;
    QString storeError;
    const auto loadedPresets = store.presets(&storeError);
    if (!storeError.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        return std::nullopt;
    }

    for (const auto& preset : loadedPresets) {
        presets.append(FieldPresetJson::toJson(preset));
    }
    return presets;
}

QJsonObject paperSpecObjectFromRequest(const QJsonObject& root)
{
    // 管理界面可提交裸纸张规格对象，也可提交 { paperSpec: ... } 包装对象。
    if (root.contains("paperSpec") && root["paperSpec"].isObject()) {
        return root["paperSpec"].toObject();
    }
    return root;
}

QJsonObject fieldPresetObjectFromRequest(const QJsonObject& root)
{
    // 管理界面可提交裸字段预设对象，也可提交 { fieldPreset: ... } 包装对象。
    if (root.contains("fieldPreset") && root["fieldPreset"].isObject()) {
        return root["fieldPreset"].toObject();
    }
    return root;
}

std::optional<PaperSpec> validatedPaperSpecFromRequest(
    const QJsonObject& root,
    QString* errorMessage)
{
    const auto paperSpecObject = paperSpecObjectFromRequest(root);
    QString validationError;
    if (!PaperSpecJson::validate(paperSpecObject, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }
    return PaperSpecJson::fromJson(paperSpecObject);
}

std::optional<FieldPreset> validatedFieldPresetFromRequest(
    const QJsonObject& root,
    QString* errorMessage)
{
    const auto presetObject = fieldPresetObjectFromRequest(root);
    QString validationError;
    if (!FieldPresetJson::validate(presetObject, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }
    return FieldPresetJson::fromJson(presetObject);
}

QJsonObject templateObjectFromRequest(const QJsonObject& root)
{
    // 前端既可以提交裸模板，也可以提交带 template 包装的对象，便于复用表单状态。
    if (root.contains("template") && root["template"].isObject()) {
        return root["template"].toObject();
    }
    return root;
}

std::optional<TemplateDocument> validatedTemplateFromRequest(
    const QJsonObject& root,
    QString* errorMessage)
{
    const auto templateObject = templateObjectFromRequest(root);
    QString validationError;
    if (!TemplateDocumentJson::validateForImport(templateObject, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }
    return TemplateDocumentJson::fromJson(templateObject);
}

} // 匿名命名空间

TemplateRoutes::TemplateRoutes(QString settingsPath)
    : m_settingsPath(std::move(settingsPath))
{
}

std::optional<LocalHttpResponse> TemplateRoutes::route(
    const LocalHttpRequest& request,
    const QHash<QByteArray, QByteArray>& extraHeaders) const
{
    const auto method = request.method.toUpper();
    const auto path = request.path;

    const auto routePaperSpecId = paperSpecIdFromPath(path);
    if (path == "/paper-specs" && method == "GET") {
        const auto store = paperSpecStoreForSettings(m_settingsPath);
        QString errorMessage;
        const auto paperSpecs = paperSpecListJson(store, &errorMessage);
        if (!paperSpecs.has_value()) {
            return jsonResponse(failEnvelope("PAPER_SPEC_STORE_ERROR", errorMessage), 500, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"paperSpecs", paperSpecs.value()},
        }), 200, extraHeaders);
    }

    if (!routePaperSpecId.isEmpty() && method == "GET") {
        const auto store = paperSpecStoreForSettings(m_settingsPath);
        QString errorMessage;
        const auto spec = store.loadPaperSpec(routePaperSpecId, &errorMessage);
        if (!spec.has_value()) {
            return jsonResponse(failEnvelope("PAPER_SPEC_NOT_FOUND", QString::fromUtf8("未找到指定纸张规格。")), 404, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"paperSpec", PaperSpecJson::toJson(spec.value())},
        }), 200, extraHeaders);
    }

    if (!routePaperSpecId.isEmpty() && method == "PUT") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid paper spec JSON"), 400, extraHeaders);
        }

        QString errorMessage;
        const auto spec = validatedPaperSpecFromRequest(parsedDocument.object(), &errorMessage);
        if (!spec.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }
        if (spec->id != routePaperSpecId) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("纸张规格 id 与路径不一致。")), 400, extraHeaders);
        }

        const auto store = paperSpecStoreForSettings(m_settingsPath);
        if (!store.savePaperSpec(spec.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"paperSpec", PaperSpecJson::toJson(spec.value())},
        }), 200, extraHeaders);
    }

    const auto routeFieldPresetId = fieldPresetIdFromPath(path);
    if (path == "/field-presets" && method == "GET") {
        QString errorMessage;
        const auto presets = fieldPresetListJson(fieldPresetStoreForSettings(m_settingsPath), &errorMessage);
        if (!presets.has_value()) {
            return jsonResponse(failEnvelope("FIELD_PRESET_STORE_ERROR", errorMessage), 500, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"fieldPresets", presets.value()},
        }), 200, extraHeaders);
    }

    if (!routeFieldPresetId.isEmpty() && method == "GET") {
        QString errorMessage;
        const auto preset = fieldPresetStoreForSettings(m_settingsPath).loadPreset(routeFieldPresetId, &errorMessage);
        if (!preset.has_value()) {
            return jsonResponse(failEnvelope("FIELD_PRESET_NOT_FOUND", errorMessage), 404, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"fieldPreset", FieldPresetJson::toJson(preset.value())},
        }), 200, extraHeaders);
    }

    if (!routeFieldPresetId.isEmpty() && method == "PUT") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid field preset JSON"), 400, extraHeaders);
        }

        QString errorMessage;
        const auto preset = validatedFieldPresetFromRequest(parsedDocument.object(), &errorMessage);
        if (!preset.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }
        if (preset->id != routeFieldPresetId) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("字段预设 id 与路径不一致。")), 400, extraHeaders);
        }

        const auto store = fieldPresetStoreForSettings(m_settingsPath);
        if (!store.savePreset(preset.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"fieldPreset", FieldPresetJson::toJson(preset.value())},
        }), 200, extraHeaders);
    }

    if (!routeFieldPresetId.isEmpty() && method == "DELETE") {
        QString errorMessage;
        const auto store = fieldPresetStoreForSettings(m_settingsPath);
        if (!store.removePreset(routeFieldPresetId, &errorMessage)) {
            return jsonResponse(failEnvelope("DELETE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"id", routeFieldPresetId},
            {"removed", true},
        }), 200, extraHeaders);
    }

    const auto routeTemplateId = templateIdFromPath(path);
    if (path == "/templates" && method == "GET") {
        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        return jsonResponse(okEnvelope(QJsonObject{
            {"templates", templateSummaryListJson(store)},
        }), 200, extraHeaders);
    }

    if (path == "/templates" && method == "POST") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid template JSON"), 400, extraHeaders);
        }

        // 保存入口接受裸模板对象，也接受 { "template": ... } 包装，方便前端表单提交。
        QString errorMessage;
        const auto templateDocument = validatedTemplateFromRequest(parsedDocument.object(), &errorMessage);
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }

        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        if (!store.saveTemplate(templateDocument.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"template", TemplateDocumentJson::toJson(templateDocument.value())},
        }), 200, extraHeaders);
    }

    if (!routeTemplateId.isEmpty() && method == "GET") {
        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        QString errorMessage;
        const auto templateDocument = store.loadTemplate(routeTemplateId, &errorMessage);
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("TEMPLATE_NOT_FOUND", QString::fromUtf8("未找到指定模板。")), 404, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"template", TemplateDocumentJson::toJson(templateDocument.value())},
        }), 200, extraHeaders);
    }

    if (!routeTemplateId.isEmpty() && method == "PUT") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid template JSON"), 400, extraHeaders);
        }

        QString errorMessage;
        const auto templateDocument = validatedTemplateFromRequest(parsedDocument.object(), &errorMessage);
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }
        if (templateDocument->id != routeTemplateId) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("模板 id 与路径不一致。")), 400, extraHeaders);
        }

        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        if (!store.saveTemplate(templateDocument.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"template", TemplateDocumentJson::toJson(templateDocument.value())},
        }), 200, extraHeaders);
    }

    if (!routeTemplateId.isEmpty() && method == "DELETE") {
        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        if (!store.removeTemplate(routeTemplateId)) {
            return jsonResponse(failEnvelope("DELETE_FAILED", QString::fromUtf8("删除模板失败。")), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"id", routeTemplateId},
            {"removed", true},
        }), 200, extraHeaders);
    }

    return std::nullopt;
}

} // 命名空间 sleekpr::http
