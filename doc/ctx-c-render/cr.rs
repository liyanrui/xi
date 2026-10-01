use std::io::{self, BufRead, Write, BufWriter};
use std::str;

const KEYWORD_COLOR: &[u8] = b"darkblue";
const FUNC_COLOR: &[u8] = b"magenta";
const STRING_COLOR: &[u8] = b"darkgreen";
const COMMENT_COLOR: &[u8] = b"darkyellow";
const NUMBER_COLOR: &[u8] = b"darkred";

const KEYWORDS: &[&str] = &[
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof",
    "static", "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while",
];

struct State {
    in_code: bool,
    in_multi: bool,
    in_string: u8,
    escape_state: bool,
    in_header: bool,
    header_lines: Vec<String>,
}

impl State {
    fn new() -> Self {
        Self {
            in_code: false,
            in_multi: false,
            in_string: 0,
            escape_state: false,
            in_header: false,
            header_lines: Vec::new(),
        }
    }
}

// 写入颜色起始
fn push_color_begin(buf: &mut Vec<u8>, color: &[u8]) {
    buf.extend_from_slice(b"/BTEX\\color[");
    buf.extend_from_slice(color);
    buf.extend_from_slice(b"]{");
}

fn push_color_end(buf: &mut Vec<u8>) {
    buf.extend_from_slice(b"}/ETEX");
}

// 字节转义（注释内，空格转\space{}）
fn push_comment_byte(buf: &mut Vec<u8>, b: u8) {
    match b {
        b' ' => buf.extend_from_slice(b"\\space{}"),
        b'{' => buf.extend_from_slice(b"\\textbraceleft{}"),
        b'}' => buf.extend_from_slice(b"\\textbraceright{}"),
        b'\\' => buf.extend_from_slice(b"\\textbackslash{}"),
        b'%' => buf.extend_from_slice(b"\\textpercent{}"),
        b'#' => buf.extend_from_slice(b"\\texthash{}"),
        b'|' => buf.extend_from_slice(b"\\textbar{}"),
        b'$' => buf.extend_from_slice(b"\\textdollar{}"),
        b'~' => buf.extend_from_slice(b"\\textasciitilde{}"),
        b'^' => buf.extend_from_slice(b"\\textasciicircum{}"),
        b'_' => buf.extend_from_slice(b"\\textunderscore{}"),
        b'&' => buf.extend_from_slice(b"\\textampersand{}"),
        _ => buf.push(b),
    }
}

// 字符串/字符内转义（不转义空格）
fn push_string_byte(buf: &mut Vec<u8>, b: u8) {
    match b {
        b'{' => buf.extend_from_slice(b"\\textbraceleft{}"),
        b'}' => buf.extend_from_slice(b"\\textbraceright{}"),
        b'\\' => buf.extend_from_slice(b"\\textbackslash{}"),
        b'%' => buf.extend_from_slice(b"\\textpercent{}"),
        b'#' => buf.extend_from_slice(b"\\texthash{}"),
        b'|' => buf.extend_from_slice(b"\\textbar{}"),
        b'$' => buf.extend_from_slice(b"\\textdollar{}"),
        b'~' => buf.extend_from_slice(b"\\textasciitilde{}"),
        b'^' => buf.extend_from_slice(b"\\textasciicircum{}"),
        b'_' => buf.extend_from_slice(b"\\textunderscore{}"),
        b'&' => buf.extend_from_slice(b"\\textampersand{}"),
        _ => buf.push(b),
    }
}

// 轻量转义（仅 BTEX 分组敏感字符）
fn push_light_escaped(buf: &mut Vec<u8>, token: &[u8]) {
    for &b in token {
        match b {
            b'{' => buf.extend_from_slice(b"\\textbraceleft{}"),
            b'}' => buf.extend_from_slice(b"\\textbraceright{}"),
            b'\\' => buf.extend_from_slice(b"\\textbackslash{}"),
            b'%' => buf.extend_from_slice(b"\\textpercent{}"),
            b'#' => buf.extend_from_slice(b"\\texthash{}"),
            _ => buf.push(b),
        }
    }
}

fn is_ident_byte(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_'
}

fn is_digit_or_num_part(b: u8) -> bool {
    b.is_ascii_digit() || b == b'x' || b == b'X' || b == b'.' || b == b'+' || b == b'-' || b == b'e' || b == b'E'
        || (b'a'..=b'f').contains(&b) || (b'A'..=b'F').contains(&b)
}

fn highlight_line_bytes(line: &[u8], state: &mut State, out: &mut Vec<u8>) {
    if state.in_multi {
        push_color_begin(out, COMMENT_COLOR);
    }
    if state.in_string == 1 || state.in_string == 2 {
        push_color_begin(out, STRING_COLOR);
    }

    let n = line.len();
    let mut i = 0;
    while i < n {
        let b = line[i];

        // Multi-line comment
        if state.in_multi {
            if b == b'*' && i + 1 < n && line[i + 1] == b'/' {
                push_comment_byte(out, b'*');
                push_comment_byte(out, b'/');
                push_color_end(out);
                state.in_multi = false;
                i += 2;
                continue;
            } else {
                push_comment_byte(out, b);
                i += 1;
                continue;
            }
        }

        // String
        if state.in_string == 1 {
            if state.escape_state {
                push_string_byte(out, b);
                state.escape_state = false;
            } else if b == b'\\' {
                push_string_byte(out, b);
                state.escape_state = true;
            } else if b == b'"' {
                push_string_byte(out, b);
                push_color_end(out);
                state.in_string = 0;
            } else {
                push_string_byte(out, b);
            }
            i += 1;
            continue;
        }

        // Char
        if state.in_string == 2 {
            if state.escape_state {
                push_string_byte(out, b);
                state.escape_state = false;
            } else if b == b'\\' {
                push_string_byte(out, b);
                state.escape_state = true;
            } else if b == b'\'' {
                push_string_byte(out, b);
                push_color_end(out);
                state.in_string = 0;
            } else {
                push_string_byte(out, b);
            }
            i += 1;
            continue;
        }

        // Normal code
        if b == b'/' {
            if i + 1 < n {
                let next = line[i + 1];
                if next == b'/' {
                    push_color_begin(out, COMMENT_COLOR);
                    push_comment_byte(out, b'/');
                    push_comment_byte(out, b'/');
                    i += 2;
                    while i < n {
                        push_comment_byte(out, line[i]);
                        i += 1;
                    }
                    push_color_end(out);
                    break;
                } else if next == b'*' {
                    state.in_multi = true;
                    push_color_begin(out, COMMENT_COLOR);
                    push_comment_byte(out, b'/');
                    push_comment_byte(out, b'*');
                    i += 2;
                    continue;
                } else {
                    out.push(b);
                }
            } else {
                out.push(b);
            }
        } else if b == b'"' {
            state.in_string = 1;
            push_color_begin(out, STRING_COLOR);
            push_string_byte(out, b);
        } else if b == b'\'' {
            state.in_string = 2;
            push_color_begin(out, STRING_COLOR);
            push_string_byte(out, b);
        } else if b.is_ascii_alphabetic() || b == b'_' {
            let start = i;
            while i < n && is_ident_byte(line[i]) {
                i += 1;
            }
            let token_bytes = &line[start..i];
            i -= 1; // loop will increment

            // Look ahead for '(' to determine function
            let mut j = i + 1;
            while j < n && (line[j] == b' ' || line[j] == b'\t') {
                j += 1;
            }
            let token_str = std::str::from_utf8(token_bytes).unwrap_or("");
            if j < n && line[j] == b'(' && !KEYWORDS.contains(&token_str) {
                push_color_begin(out, FUNC_COLOR);
                push_light_escaped(out, token_bytes);
                push_color_end(out);
            } else if KEYWORDS.contains(&token_str) {
                push_color_begin(out, KEYWORD_COLOR);
                push_light_escaped(out, token_bytes);
                push_color_end(out);
            } else {
                out.extend_from_slice(token_bytes);
            }
        } else if b.is_ascii_digit() {
            let start = i;
            while i < n && is_digit_or_num_part(line[i]) {
                i += 1;
            }
            let token_bytes = &line[start..i];
            i -= 1;
            push_color_begin(out, NUMBER_COLOR);
            push_light_escaped(out, token_bytes);
            push_color_end(out);
        } else {
            out.push(b);
        }
        i += 1;
    }

    if state.in_multi {
        push_color_end(out);
    }
    if state.in_string == 1 || state.in_string == 2 {
        push_color_end(out);
    }
}

// 其余辅助函数
fn is_single_at(line: &[u8]) -> bool {
    let trimmed = match std::str::from_utf8(line) {
        Ok(s) => s.trim(),
        Err(_) => return false,
    };
    trimmed == "@"
}

fn is_reference_line(line: &[u8]) -> bool {
    let s = match std::str::from_utf8(line) {
        Ok(s) => s,
        Err(_) => return false,
    };
    let trimmed = s.trim_start();
    if !trimmed.starts_with('#') {
        return false;
    }
    if let Some(pos) = trimmed.rfind('@') {
        let after = &trimmed[pos + 1..];
        if after.trim().is_empty() {
            let before = &trimmed[1..pos];
            if before.contains('@') {
                return false;
            }
            return true;
        }
    }
    false
}

fn process_line(line: &str, state: &mut State, writer: &mut impl Write) {
    // 对于跨行头等，还是用字符串方便
    if state.in_header {
        if line.contains('#') {
            state.header_lines.push(line.to_string());
            let full = state.header_lines.join("\n");
            writeln!(writer, "{}", full).unwrap();
            state.header_lines.clear();
            state.in_header = false;
            state.in_code = true;
        } else {
            state.header_lines.push(line.to_string());
        }
        return;
    }

    let line_bytes = line.as_bytes();

    if state.in_code && is_single_at(line_bytes) {
        state.in_code = false;
        writeln!(writer, "{}", line).unwrap();
        return;
    }

    if state.in_code {
        if is_reference_line(line_bytes) {
            writeln!(writer, "{}", line).unwrap();
            return;
        }
        let mut out_buf: Vec<u8> = Vec::with_capacity(line.len() * 2);
        highlight_line_bytes(line_bytes, state, &mut out_buf);
        writer.write_all(&out_buf).unwrap();
        writer.write_all(b"\n").unwrap();
        return;
    }

    if line.starts_with('@') {
        if line.contains('#') {
            writeln!(writer, "{}", line).unwrap();
            state.in_code = true;
        } else {
            state.in_header = true;
            state.header_lines.push(line.to_string());
        }
        return;
    }

    writeln!(writer, "{}", line).unwrap();
}

fn main() {
    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());
    let mut state = State::new();

    for line_result in stdin.lock().lines() {
        match line_result {
            Ok(line) => process_line(&line, &mut state, &mut writer),
            Err(e) => {
                eprintln!("Error: {}", e);
                break;
            }
        }
    }

    if state.in_header && !state.header_lines.is_empty() {
        let full = state.header_lines.join("\n");
        writeln!(writer, "{}", full).unwrap();
    }
    writer.flush().unwrap();
}