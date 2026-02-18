#include "path_templates.h"

#include <string>

namespace ProcessInterface {
namespace Common {

namespace {

void ReplaceAll(std::string& text, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) {
        return;
    }

    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        text.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

}  // namespace

bool ValidateTemplateHasToken(
    const std::string& template_text,
    const std::string& token_name,
    std::string& error_message) {
    const std::string token = "{" + token_name + "}";
    if (template_text.find(token) != std::string::npos) {
        return true;
    }

    error_message = "missing required token " + token + " in template";
    return false;
}

std::string RenderTemplate(const std::string& template_text, const PathTemplateArgs& args) {
    std::string rendered = template_text;
    ReplaceAll(rendered, "{repoRoot}", args.repo_root);
    ReplaceAll(rendered, "{appId}", args.app_id);
    ReplaceAll(rendered, "{jobId}", args.job_id);
    return rendered;
}

fs::path RenderTemplatePath(const std::string& template_text, const PathTemplateArgs& args) {
    return fs::path(RenderTemplate(template_text, args));
}

}  // namespace Common
}  // namespace ProcessInterface
