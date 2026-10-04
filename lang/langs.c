#include "langs.h"
#include "ed.h"
#include "hl.h"

const struct TSLanguage *tree_sitter_c(void);
const struct TSLanguage *tree_sitter_cpp(void);
const struct TSLanguage *tree_sitter_c_sharp(void);
const struct TSLanguage *tree_sitter_css(void);
const struct TSLanguage *tree_sitter_javascript(void);
const struct TSLanguage *tree_sitter_html(void);
const struct TSLanguage *tree_sitter_lua(void);
const struct TSLanguage *tree_sitter_apex(void);
const struct TSLanguage *tree_sitter_python(void);
const struct TSLanguage *tree_sitter_ruby(void);
const struct TSLanguage *tree_sitter_rust(void);
const struct TSLanguage *tree_sitter_markdown(void);
const struct TSLanguage *tree_sitter_json(void);
const struct TSLanguage *tree_sitter_xml(void);
const struct TSLanguage *tree_sitter_bash(void);
const struct TSLanguage *tree_sitter_powershell(void);
const struct TSLanguage *tree_sitter_markdown_inline(void);

extern const char hl_query_c[];
extern const char hl_query_cpp[];
extern const char hl_query_csharp[];
extern const char hl_query_css[];
extern const char hl_query_javascript[];
extern const char hl_query_html[];
extern const char hl_query_lua[];
extern const char hl_query_apex[];
extern const char hl_query_python[];
extern const char hl_query_ruby[];
extern const char hl_query_rust[];
extern const char hl_query_markdown[];
extern const char hl_query_json[];
extern const char hl_query_xml[];
extern const char hl_query_bash[];
extern const char hl_query_powershell[];
extern const char hl_query_markdowninline[];

enum {
    L_C,
    L_CPP,
    L_CSHARP,
    L_CSS,
    L_JAVASCRIPT,
    L_HTML,
    L_LUA,
    L_APEX,
    L_PYTHON,
    L_RUBY,
    L_RUST,
    L_MARKDOWN,
    L_JSON,
    L_XML,
    L_BASH,
    L_POWERSHELL,
    L_MARKDOWN_INLINE,
    L_SHIPPED
};

static const struct hl_inner in_html[] = {
    { "raw_text", "script_element", L_JAVASCRIPT, 1, 0 },
    { "raw_text", "style_element", L_CSS, 1, 0 },
};

static const struct hl_inner in_markdown[] = {
    { "inline", 0, L_MARKDOWN_INLINE, 0, 0 },
    { 0, 0, HL_NAMED, 0, "(fenced_code_block (info_string (language) @n) (code_fence_content) @i)" },
    { "html_block", 0, L_HTML, 0, 0 },
};

struct hl_lang hl_langs[LANGS_MAX] = {
    { tree_sitter_c, hl_query_c, 0, 0u, "c h" },
    { tree_sitter_cpp, hl_query_cpp, 0, 0u, "cpp c++ cxx cc hpp" },
    { tree_sitter_c_sharp, hl_query_csharp, 0, 0u, "csharp cs c#" },
    { tree_sitter_css, hl_query_css, 0, 0u, "css" },
    { tree_sitter_javascript, hl_query_javascript, 0, 0u, "javascript js jsx mjs" },
    { tree_sitter_html, hl_query_html, in_html, 2u, "html htm" },
    { tree_sitter_lua, hl_query_lua, 0, 0u, "lua" },
    { tree_sitter_apex, hl_query_apex, 0, 0u, "apex" },
    { tree_sitter_python, hl_query_python, 0, 0u, "python py python3" },
    { tree_sitter_ruby, hl_query_ruby, 0, 0u, "ruby rb" },
    { tree_sitter_rust, hl_query_rust, 0, 0u, "rust rs" },
    { tree_sitter_markdown, hl_query_markdown, in_markdown, 3u, "markdown md" },
    { tree_sitter_json, hl_query_json, 0, 0u, "json" },
    { tree_sitter_xml, hl_query_xml, 0, 0u, "xml svg xsd xsl" },
    { tree_sitter_bash, hl_query_bash, 0, 0u, "bash sh shell zsh" },
    { tree_sitter_powershell, hl_query_powershell, 0, 0u, "powershell ps1 pwsh" },
    { tree_sitter_markdown_inline, hl_query_markdowninline, 0, 0u, 0 },
};

uint32_t hl_lang_count = L_SHIPPED;

struct ed_ext ed_exts[LANGS_EXT_MAX] = {
    { ".c", "c", "C", "C source file", L_C, 0u },
    { ".h", "c", "C", "C source file", L_C, 0u },
    { ".cpp", "cpp", "C++", "C++ source file", L_CPP, 0u },
    { ".cc", "cpp", "C++", "C++ source file", L_CPP, 0u },
    { ".cxx", "cpp", "C++", "C++ source file", L_CPP, 0u },
    { ".hpp", "cpp", "C++", "C++ source file", L_CPP, 0u },
    { ".hh", "cpp", "C++", "C++ source file", L_CPP, 0u },
    { ".hxx", "cpp", "C++", "C++ source file", L_CPP, 0u },
    { ".cs", "csharp", "C#", "C# source file", L_CSHARP, 0u },
    { ".css", "css", "CSS", "Cascading Style Sheets file", L_CSS, 0u },
    { ".js", "javascript", "JavaScript", "JavaScript file", L_JAVASCRIPT, 0u },
    { ".mjs", "javascript", "JavaScript", "JavaScript file", L_JAVASCRIPT, 0u },
    { ".cjs", "javascript", "JavaScript", "JavaScript file", L_JAVASCRIPT, 0u },
    { ".jsx", "javascript", "JavaScript", "JavaScript file", L_JAVASCRIPT, 0u },
    { ".html", "html", "HTML", "HTML file", L_HTML, 0u },
    { ".htm", "html", "HTML", "HTML file", L_HTML, 0u },
    { ".lua", "lua", "Lua", "Lua source file", L_LUA, 0u },
    { ".cls", "apex", "Apex", "Salesforce Apex file", L_APEX, 0u },
    { ".trigger", "apex", "Apex", "Salesforce Apex file", L_APEX, 0u },
    { ".apex", "apex", "Apex", "Salesforce Apex file", L_APEX, 0u },
    { ".py", "python", "Python", "Python file", L_PYTHON, 0u },
    { ".pyw", "python", "Python", "Python file", L_PYTHON, 0u },
    { ".rb", "ruby", "Ruby", "Ruby file", L_RUBY, 0u },
    { ".rake", "ruby", "Ruby", "Ruby file", L_RUBY, 0u },
    { ".gemspec", "ruby", "Ruby", "Ruby file", L_RUBY, 0u },
    { ".rs", "rust", "Rust", "Rust source file", L_RUST, 0u },
    { ".md", "markdown", "Markdown", "Markdown file", L_MARKDOWN, 0u },
    { ".markdown", "markdown", "Markdown", "Markdown file", L_MARKDOWN, 0u },
    { ".json", "json", "JSON", "JSON file", L_JSON, 0u },
    { ".xml", "xml", "XML", "XML file", L_XML, 0u },
    { ".xsd", "xml", "XML", "XML file", L_XML, 0u },
    { ".xsl", "xml", "XML", "XML file", L_XML, 0u },
    { ".xslt", "xml", "XML", "XML file", L_XML, 0u },
    { ".svg", "xml", "XML", "XML file", L_XML, 0u },
    { ".sh", "bash", "Bash", "Shell script", L_BASH, 0u },
    { ".bash", "bash", "Bash", "Shell script", L_BASH, 0u },
    { ".ps1", "powershell", "PowerShell", "PowerShell script", L_POWERSHELL, 0u },
    { ".psm1", "powershell", "PowerShell", "PowerShell script", L_POWERSHELL, 0u },
    { ".psd1", "powershell", "PowerShell", "PowerShell script", L_POWERSHELL, 0u },
    { ".csv", "csv", "CSV", "Comma-separated values file", HL_PLAIN, ',' },
    { ".tsv", "tsv", "TSV", "Tab-separated values file", HL_PLAIN, '\t' },
};

uint32_t ed_ext_count = 41u;
