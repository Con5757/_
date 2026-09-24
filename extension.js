const vscode = require("vscode");

const UNDERSCORE_LANGUAGES = new Set([
  "u",
  "u-lang",
  "uscore",
  "progu",
  "_",
  "score",
  "underscore",
]);

function escapeHtml(text) {
  return text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function span(className, text) {
  return `<span class="${className}">${escapeHtml(text)}</span>`;
}

function isWordStart(c) {
  return /[A-Za-z_]/.test(c);
}

function isWordChar(c) {
  return /[A-Za-z0-9_]/.test(c);
}

function readWord(text, start) {
  let end = start + 1;

  while (end < text.length && isWordChar(text[end])) {
    end++;
  }

  return {
    value: text.slice(start, end),
    end,
  };
}

function nextNonWhitespace(text, start) {
  let i = start;

  while (i < text.length && /\s/.test(text[i])) {
    i++;
  }

  return i;
}

function findStringEnd(text, start, quote) {
  for (let i = start + 1; i < text.length; i++) {
    if (text[i] === "\\") {
      i++;
      continue;
    }

    if (text[i] === quote) {
      return i;
    }
  }

  return text.length - 1;
}

function highlightUnderscore(source) {
  let html = "";
  let i = 0;

  while (i < source.length) {
    /*
     * Multiline comments
     */
    if (source.startsWith("!..", i)) {
      const end = source.indexOf("..!", i + 3);

      if (end === -1) {
        html += span("u-comment", source.slice(i));
        break;
      }

      html += span("u-comment", source.slice(i, end + 3));

      i = end + 3;
      continue;
    }

    /*
     * Single-line comments
     */
    if (source.startsWith("..", i) && !source.startsWith("..!", i)) {
      const end = source.indexOf("\n", i);

      if (end === -1) {
        html += span("u-comment", source.slice(i));
        break;
      }

      html += span("u-comment", source.slice(i, end));

      i = end;
      continue;
    }

    /*
     * Strings
     */
    if (source[i] === '"' || source[i] === "'") {
      const quote = source[i];
      const end = findStringEnd(source, i, quote);

      html += span("u-string", source.slice(i, end + 1));

      i = end + 1;
      continue;
    }

    /*
     * Imports
     * ?std
     */
    if (source[i] === "?" && isWordStart(source[i + 1] || "")) {
      const word = readWord(source, i + 1);

      html += span("u-import", source.slice(i, word.end));

      i = word.end;
      continue;
    }

    /*
     * Pointers
     */
    if (source.startsWith("-->", i)) {
      html += span("u-pointer", "-->");
      i += 3;
      continue;
    }

    if (source.startsWith("<--", i)) {
      html += span("u-pointer", "<--");
      i += 3;
      continue;
    }

    if (source.startsWith("->", i)) {
      html += span("u-pointer", "->");
      i += 2;
      continue;
    }

    if (source.startsWith("<-", i)) {
      html += span("u-pointer", "<-");
      i += 2;
      continue;
    }

    /*
     * Function flags
     * <void>
     * <int>
     * <fp>
     * <str>
     * <>
     */
    if (source[i] === "<" && source[i + 1] !== "-") {
      const lineEnd = source.indexOf("\n", i);
      const endOfLine = lineEnd === -1 ? source.length : lineEnd;

      const close = source.indexOf(">", i + 1);

      if (close !== -1 && close < endOfLine) {
        html += span("u-flag-delimiter", "<");

        const contents = source.slice(i + 1, close);

        let j = 0;

        while (j < contents.length) {
          const typeMatch = contents.slice(j).match(/^(int|fp|str|void)\b/);

          if (typeMatch) {
            html += span("u-type", typeMatch[1]);

            j += typeMatch[1].length;
            continue;
          }

          html += escapeHtml(contents[j]);
          j++;
        }

        html += span("u-flag-delimiter", ">");

        i = close + 1;
        continue;
      }
    }

    /*
     * Command flags
     * -silent
     */
    if (source[i] === "-" && /[A-Za-z_]/.test(source[i + 1] || "")) {
      let end = i + 2;

      while (end < source.length && /[A-Za-z0-9_]/.test(source[end])) {
        end++;
      }

      html += span("u-command-flag", source.slice(i, end));

      i = end;
      continue;
    }

    /*
     * Arithmetic operators
     */
    if ("+-*/%".includes(source[i])) {
      html += span("u-operator", source[i]);

      i++;
      continue;
    }

    /*
     * Numbers
     */
    if (/[0-9]/.test(source[i])) {
      const match = source.slice(i).match(/^[0-9]+(?:\.[0-9]+)?/);

      if (match) {
        html += span("u-number", match[0]);

        i += match[0].length;
        continue;
      }
    }

    /*
     * Words
     */
    if (isWordStart(source[i])) {
      const word = readWord(source, i);
      const value = word.value;

      /*
       * Types
       */
      if (/^(int|fp|str|void)$/.test(value)) {
        html += span("u-type", value);
        i = word.end;
        continue;
      }

      /*
       * Booleans
       */
      if (/^(true|false)$/.test(value)) {
        html += span("u-boolean", value);
        i = word.end;
        continue;
      }

      /*
       * const
       */
      if (value === "const") {
        html += span("u-keyword", value);
        i = word.end;
        continue;
      }

      /*
       * with
       */
      if (value === "with") {
        html += span("u-import", value);
        i = word.end;
        continue;
      }

      /*
       * Libraries
       */
      if (/^(terminal|boolean|math|string|file)$/.test(value)) {
        html += span("u-module", value);
        i = word.end;
        continue;
      }

      /*
       * std.out
       * std.in
       * std.nl
       */
      if (value === "std" && source[word.end] === ".") {
        const memberStart = word.end + 1;

        const member = source.slice(memberStart).match(/^(out|in|nl)\b/);

        if (member) {
          html += span("u-module", "std");
          html += ".";
          html += span("u-library-function", member[1]);

          i = memberStart + member[1].length;

          continue;
        }
      }

      /*
       * String functions
       */
      if (
        /^(length|copy|overwrite|compare|subSearch|slice|sliceArray|join|up|low|trim)$/.test(
          value,
        )
      ) {
        const next = nextNonWhitespace(source, word.end);

        if (source[next] === "(") {
          html += span("u-string-function", value);

          i = word.end;
          continue;
        }
      }

      /*
       * func
       */
      if (value === "func") {
        html += span("u-function-keyword", value);

        i = word.end;
        continue;
      }

      /*
       * Control keywords
       */
      if (
        /^(if|elif|else|during|repeat|switch|arg|incase|attempt|end|break|continue|retreat|kill|main|as)$/.test(
          value,
        )
      ) {
        html += span("u-keyword", value);

        i = word.end;
        continue;
      }

      /*
       * Function calls
       */
      const next = nextNonWhitespace(source, word.end);

      if (source[next] === "(") {
        html += span("u-function", value);

        i = word.end;
        continue;
      }

      /*
       * Normal text
       */
      html += escapeHtml(value);
      i = word.end;
      continue;
    }

    /*
     * Everything else
     */
    html += escapeHtml(source[i]);
    i++;
  }

  return html;
}

function activate(context) {
  console.log("Underscore Markdown preview activated");

  return {
    extendMarkdownIt(md) {
      const originalFence = md.renderer.rules.fence;

      md.renderer.rules.fence = function (tokens, idx, options, env, self) {
        const token = tokens[idx];

        const language = token.info.trim().split(/\s+/)[0].toLowerCase();

        /*
         * Let normal Markdown handle
         * every language except Underscore.
         */
        if (!UNDERSCORE_LANGUAGES.has(language)) {
          return originalFence
            ? originalFence(tokens, idx, options, env, self)
            : self.renderToken(tokens, idx, options);
        }

        /*
         * Render Underscore ourselves.
         */
        const highlighted = highlightUnderscore(token.content);

        return (
          `<pre class="u-preview-block">` +
          `<code class="language-${escapeHtml(language)} underscore-preview-code">` +
          highlighted +
          `</code>` +
          `</pre>`
        );
      };

      return md;
    },
  };
}

function deactivate() {}

module.exports = {
  activate,
  deactivate,
};