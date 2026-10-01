/*
 * litprog_highlight_opt.cpp
 * Optimized C++ version.
 * Compile: g++ -std=c++17 -O2 -o litprog_highlight litprog_highlight_opt.cpp
 */
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>
#include <algorithm>

/* ---------- colours ---------- */
constexpr std::string_view KEYWORD_COLOR = "darkblue";
constexpr std::string_view FUNC_COLOR    = "magenta";
constexpr std::string_view STRING_COLOR  = "darkgreen";
constexpr std::string_view COMMENT_COLOR = "darkyellow";
constexpr std::string_view NUMBER_COLOR  = "darkred";

/* ---------- C keywords (sorted for faster linear search? small anyway) ---------- */
constexpr std::string_view KEYWORDS[] = {
    "auto","break","case","char","const","continue","default","do",
    "double","else","enum","extern","float","for","goto","if",
    "int","long","register","return","short","signed","sizeof",
    "static","struct","switch","typedef","union","unsigned","void",
    "volatile","while"
};

constexpr size_t NUM_KEYWORDS = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);

static bool is_keyword(std::string_view word) {
    for (auto &kw : KEYWORDS) {
        if (kw == word) return true;
    }
    return false;
}

/* ---------- state ---------- */
struct State {
    bool in_code = false;
    bool in_multi = false;
    int  in_string = 0;      // 0:none, 1:double, 2:single
    bool escape_state = false;
    bool in_header = false;
    std::vector<std::string> header_lines;
};

/* ---------- output buffer (std::string with reserve) ---------- */
// We will use a simple std::string as buffer; in highlight_line we'll append directly.

inline void begin_color(std::string &buf, std::string_view color) {
    buf += "/BTEX\\color[";
    buf += color;
    buf += "]{";
}

inline void end_color(std::string &buf) {
    buf += "}/ETEX";
}

/* ---------- escaping to buffer ---------- */
void escape_comment_char(std::string &buf, char c) {
    switch (c) {
        case ' ': buf += "\\space{}"; break;
        case '{': buf += "\\textbraceleft{}"; break;
        case '}': buf += "\\textbraceright{}"; break;
        case '\\': buf += "\\textbackslash{}"; break;
        case '%': buf += "\\textpercent{}"; break;
        case '#': buf += "\\texthash{}"; break;
        case '|': buf += "\\textbar{}"; break;
        case '$': buf += "\\textdollar{}"; break;
        case '~': buf += "\\textasciitilde{}"; break;
        case '^': buf += "\\textasciicircum{}"; break;
        case '_': buf += "\\textunderscore{}"; break;
        case '&': buf += "\\textampersand{}"; break;
        default:  buf += c;
    }
}

void escape_string_char(std::string &buf, char c) {
    switch (c) {
        case '{': buf += "\\textbraceleft{}"; break;
        case '}': buf += "\\textbraceright{}"; break;
        case '\\': buf += "\\textbackslash{}"; break;
        case '%': buf += "\\textpercent{}"; break;
        case '#': buf += "\\texthash{}"; break;
        case '|': buf += "\\textbar{}"; break;
        case '$': buf += "\\textdollar{}"; break;
        case '~': buf += "\\textasciitilde{}"; break;
        case '^': buf += "\\textasciicircum{}"; break;
        case '_': buf += "\\textunderscore{}"; break;
        case '&': buf += "\\textampersand{}"; break;
        default:  buf += c;
    }
}

void escape_light(std::string &buf, std::string_view s) {
    for (char c : s) {
        switch (c) {
            case '{': buf += "\\textbraceleft{}"; break;
            case '}': buf += "\\textbraceright{}"; break;
            case '\\': buf += "\\textbackslash{}"; break;
            case '%': buf += "\\textpercent{}"; break;
            case '#': buf += "\\texthash{}"; break;
            default:  buf += c;
        }
    }
}

/* ---------- inline character classification ---------- */
static inline bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline bool is_ident_cont(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}
static inline bool is_num_part(char c) {
    return (c >= '0' && c <= '9') ||
           c == 'x' || c == 'X' ||
           (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
           c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E';
}

/* ---------- syntax highlight (uses string_view to avoid copies) ---------- */
void highlight_line(std::string_view line, State &state, std::string &out) {
    if (state.in_multi) begin_color(out, COMMENT_COLOR);
    if (state.in_string == 1 || state.in_string == 2) begin_color(out, STRING_COLOR);

    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        char c = line[i];

        // multi-line comment
        if (state.in_multi) {
            if (c == '*' && i+1 < n && line[i+1] == '/') {
                escape_comment_char(out, '*');
                escape_comment_char(out, '/');
                end_color(out);
                state.in_multi = false;
                i += 2;
                continue;
            } else {
                escape_comment_char(out, c);
                ++i;
                continue;
            }
        }

        // double string
        if (state.in_string == 1) {
            if (state.escape_state) {
                escape_string_char(out, c);
                state.escape_state = false;
            } else if (c == '\\') {
                escape_string_char(out, c);
                state.escape_state = true;
            } else if (c == '"') {
                escape_string_char(out, c);
                end_color(out);
                state.in_string = 0;
            } else {
                escape_string_char(out, c);
            }
            ++i;
            continue;
        }

        // single char
        if (state.in_string == 2) {
            if (state.escape_state) {
                escape_string_char(out, c);
                state.escape_state = false;
            } else if (c == '\\') {
                escape_string_char(out, c);
                state.escape_state = true;
            } else if (c == '\'') {
                escape_string_char(out, c);
                end_color(out);
                state.in_string = 0;
            } else {
                escape_string_char(out, c);
            }
            ++i;
            continue;
        }

        // normal code
        if (c == '/') {
            if (i+1 < n) {
                char next = line[i+1];
                if (next == '/') {
                    begin_color(out, COMMENT_COLOR);
                    escape_comment_char(out, '/');
                    escape_comment_char(out, '/');
                    i += 2;
                    while (i < n) {
                        escape_comment_char(out, line[i]);
                        ++i;
                    }
                    end_color(out);
                    break;
                } else if (next == '*') {
                    state.in_multi = true;
                    begin_color(out, COMMENT_COLOR);
                    escape_comment_char(out, '/');
                    escape_comment_char(out, '*');
                    i += 2;
                    continue;
                } else {
                    out += c;
                }
            } else {
                out += c;
            }
        } else if (c == '"') {
            state.in_string = 1;
            begin_color(out, STRING_COLOR);
            escape_string_char(out, c);
        } else if (c == '\'') {
            state.in_string = 2;
            begin_color(out, STRING_COLOR);
            escape_string_char(out, c);
        } else if (is_ident_start(c)) {
            size_t start = i;
            while (i < n && is_ident_cont(line[i])) ++i;
            std::string_view token(line.data() + start, i - start);
            --i; // adjust for loop increment

            // look ahead for '('
            size_t j = i + 1;
            while (j < n && (line[j] == ' ' || line[j] == '\t')) ++j;
            if (j < n && line[j] == '(' && !is_keyword(token)) {
                begin_color(out, FUNC_COLOR);
                escape_light(out, token);
                end_color(out);
            } else if (is_keyword(token)) {
                begin_color(out, KEYWORD_COLOR);
                escape_light(out, token);
                end_color(out);
            } else {
                out += token;
            }
        } else if (is_digit(c)) {
            size_t start = i;
            while (i < n && is_num_part(line[i])) ++i;
            std::string_view token(line.data() + start, i - start);
            --i;
            begin_color(out, NUMBER_COLOR);
            escape_light(out, token);
            end_color(out);
        } else {
            out += c;
        }
        ++i;
    }

    if (state.in_multi) end_color(out);
    if (state.in_string == 1 || state.in_string == 2) end_color(out);
}

/* ---------- line classification (uses string_view) ---------- */
bool is_single_at(std::string_view line) {
    // left trim
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
    size_t end = line.size();
    while (end > start && (line[end-1] == ' ' || line[end-1] == '\t')) --end;
    std::string_view trimmed = line.substr(start, end - start);
    return trimmed == "@";
}

bool is_reference_line(std::string_view line) {
    // left trim
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
    if (start >= line.size() || line[start] != '#') return false;
    // find last '@'
    size_t at_pos = line.rfind('@');
    if (at_pos == std::string_view::npos || at_pos <= start) return false;
    // after '@' only spaces
    for (size_t i = at_pos+1; i < line.size(); ++i) {
        if (line[i] != ' ' && line[i] != '\t') return false;
    }
    // between '#' and '@' must not contain another '@'
    std::string_view between = line.substr(start+1, at_pos - start - 1);
    if (between.find('@') != std::string_view::npos) return false;
    return true;
}

/* ---------- main loop ---------- */
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    State state;
    std::string line;

    // Pre-allocate a buffer for output (avoid repeated reallocation in the hot loop)
    std::string out_buffer;
    out_buffer.reserve(4096); // typical line length

    while (std::getline(std::cin, line)) {
        // Remove possible trailing carriage return (Windows)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Handle continued header
        if (state.in_header) {
            if (line.find('#') != std::string::npos) {
                state.header_lines.push_back(line);
                // output full header
                for (size_t i = 0; i < state.header_lines.size(); ++i) {
                    if (i > 0) std::cout << '\n';
                    std::cout << state.header_lines[i];
                }
                std::cout << '\n';
                state.header_lines.clear();
                state.in_header = false;
                state.in_code = true;
            } else {
                state.header_lines.push_back(line);
            }
            continue;
        }

        // End of code block
        if (state.in_code && is_single_at(line)) {
            state.in_code = false;
            std::cout << line << '\n';
            continue;
        }

        // Inside code block
        if (state.in_code) {
            if (is_reference_line(line)) {
                std::cout << line << '\n';
                continue;
            }
            // Highlight into buffer
            out_buffer.clear();
            highlight_line(line, state, out_buffer);
            std::cout << out_buffer << '\n';
            continue;
        }

        // Start of code block
        if (!line.empty() && line[0] == '@') {
            if (line.find('#') != std::string::npos) {
                std::cout << line << '\n';
                state.in_code = true;
            } else {
                state.in_header = true;
                state.header_lines.push_back(line);
            }
            continue;
        }

        // Normal text
        std::cout << line << '\n';
    }

    // Flush remaining header if any
    if (state.in_header && !state.header_lines.empty()) {
        for (size_t i = 0; i < state.header_lines.size(); ++i) {
            if (i > 0) std::cout << '\n';
            std::cout << state.header_lines[i];
        }
        std::cout << '\n';
    }

    return 0;
}
