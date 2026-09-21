const vscode = require("vscode");

// Helper function to detect library aliases (e.g., 'using terminal as term' or default 'terminal')
function getLibraryAlias(text, libName) {
  const regex = new RegExp(
    `using\\s+${libName}(?:\\s+as\\s+([a-zA-Z_][a-zA-Z0-9_]*))?`,
    "i",
  );
  const match = text.match(regex);
  if (!match) return null;
  return match[1] ? match[1] : libName;
}

let markdownApi = null;

function activate(context) {
  const diagnosticsCollection =
    vscode.languages.createDiagnosticCollection("night");
  context.subscriptions.push(diagnosticsCollection);

  // --- DYNAMIC SEMANTIC TOKENS PROVIDER ---
  const tokenTypes = ["class"];
  const legend = new vscode.SemanticTokensLegend(tokenTypes);

  const semanticTokensProvider =
    vscode.languages.registerDocumentSemanticTokensProvider(
      "night",
      {
        provideDocumentSemanticTokens(document) {
          const tokensBuilder = new vscode.SemanticTokensBuilder(legend);
          const text = document.getText();

          const terminalAlias = getLibraryAlias(text, "terminal");
          if (!terminalAlias) return tokensBuilder.build();

          const lines = text.split("\n");
          lines.forEach((lineText, lineIdx) => {
            if (lineText.trim().startsWith(";")) return;

            const regex = new RegExp(
              `\\b(${terminalAlias})\\.(out|in|nl)\\b`,
              "g",
            );
            let match;
            while ((match = regex.exec(lineText)) !== null) {
              tokensBuilder.push(lineIdx, match.index, match[1].length, 0, 0);
            }
          });

          return tokensBuilder.build();
        },
      },
      legend,
    );

  context.subscriptions.push(semanticTokensProvider);

  // --- AUTOCOMPLETE & SNIPPET PROVIDER ---
  const completionProvider = vscode.languages.registerCompletionItemProvider(
    "night",
    {
      provideCompletionItems(document, position) {
        const text = document.getText();
        const linePrefix = document
          .lineAt(position)
          .text.substr(0, position.character);

        const items = [];

        // Fixed-width Integer Snippets
        const bitWidths = ["8", "16", "32", "64"];
        bitWidths.forEach((width) => {
          const item = new vscode.CompletionItem(
            `#${width}&`,
            vscode.CompletionItemKind.Snippet,
          );

          // Use a range replacement to overwrite the typed '#' rather than appending to it
          const range = document.getWordRangeAtPosition(position, /#\d*&?/);
          if (range) {
            item.range = range;
          }

          item.insertText = new vscode.SnippetString(`#${width}&$1`);
          item.detail = `Night ${width}-bit fixed integer`;
          item.documentation = new vscode.MarkdownString(
            `Declares a **${width}-bit** fixed integer variable.\n\nExample: \`#${width}&myVar\``,
          );
          item.sortText = `00_${width}`;
          item.filterText = `#${width}&`;
          items.push(item);
        });

        // Dynamic Terminal Method Matching
        const terminalAlias = getLibraryAlias(text, "terminal");
        if (terminalAlias) {
          const escapedAlias = terminalAlias.replace(
            /[.*+?^${}()|[\]\\]/g,
            "\\$&",
          );
          const termRegex = new RegExp(
            `(?:^|[^a-zA-Z0-9_])${escapedAlias}\\.([a-zA-Z_]*)$`,
          );

          if (termRegex.test(linePrefix)) {
            const outItem = new vscode.CompletionItem(
              "out",
              vscode.CompletionItemKind.Method,
            );
            outItem.insertText = new vscode.SnippetString("out(${1:})");
            outItem.detail = `Print terminal output (${terminalAlias}.out)`;
            outItem.filterText = "out";

            const inItem = new vscode.CompletionItem(
              "in",
              vscode.CompletionItemKind.Method,
            );
            inItem.insertText = new vscode.SnippetString("in(${1:})");
            inItem.detail = `Read terminal input (${terminalAlias}.in)`;
            inItem.filterText = "in";

            const nlItem = new vscode.CompletionItem(
              "nl",
              vscode.CompletionItemKind.Method,
            );
            nlItem.insertText = "nl";
            nlItem.detail = "Terminal new line";
            nlItem.filterText = "nl";

            items.push(outItem, inItem, nlItem);
          }
        }

        // Suggest 'as' keyword after typing 'using <lib> '
        if (
          /\busing\s+(terminal|boolean|string|math|file)\s+$/.test(linePrefix)
        ) {
          const asItem = new vscode.CompletionItem(
            "as",
            vscode.CompletionItemKind.Keyword,
          );
          asItem.detail = "Alias library import";
          items.push(asItem);
        }

        // Suggest available libraries after 'using '
        if (linePrefix.endsWith("using ") || linePrefix.endsWith("using")) {
          const libraries = ["boolean", "terminal", "math", "string", "file"];
          libraries.forEach((lib) => {
            const item = new vscode.CompletionItem(
              lib,
              vscode.CompletionItemKind.Module,
            );
            item.detail = `Night Library: ${lib}`;
            items.push(item);
          });
        }

        // Suggest boolean constants if boolean imported
        if (getLibraryAlias(text, "boolean")) {
          const trueItem = new vscode.CompletionItem(
            "true",
            vscode.CompletionItemKind.Keyword,
          );
          trueItem.detail = "Boolean true literal";

          const falseItem = new vscode.CompletionItem(
            "false",
            vscode.CompletionItemKind.Keyword,
          );
          falseItem.detail = "Boolean false literal";

          const nilItem = new vscode.CompletionItem(
            "nil",
            vscode.CompletionItemKind.Value,
          );
          nilItem.detail = "Nil / null literal";

          items.push(trueItem, falseItem, nilItem);
        }

        // Suggest string functions if string imported
        if (getLibraryAlias(text, "string")) {
          const stringFuncs = [
            { name: "length", body: "length(${1:})" },
            { name: "copy", body: "copy(${1:})" },
            { name: "overwrite", body: "overwrite(${1:})" },
            { name: "compare", body: "compare(${1:})" },
            { name: "subSearch", body: "subSearch(${1:})" },
            { name: "slice", body: "slice(${1:})" },
            { name: "sliceArray", body: "sliceArray(${1:})" },
            { name: "join", body: "join(${1:})" },
            { name: "up", body: "up(${1:})" },
            { name: "low", body: "low(${1:})" },
            { name: "trim", body: "trim(${1:})" },
          ];

          stringFuncs.forEach((fn) => {
            const item = new vscode.CompletionItem(
              fn.name,
              vscode.CompletionItemKind.Function,
            );
            item.insertText = new vscode.SnippetString(fn.body);
            item.detail = `String method: ${fn.name}`;
            items.push(item);
          });
        }

        // Control Flow Snippets
        const mainItem = new vscode.CompletionItem(
          "main",
          vscode.CompletionItemKind.Snippet,
        );
        mainItem.insertText = new vscode.SnippetString(
          "${1|#,%,@,$,?|}func main(${2:}) {\n\t$0\n\tkill\n}",
        );
        mainItem.detail = "Main function block";

        const funcItem = new vscode.CompletionItem(
          "func",
          vscode.CompletionItemKind.Snippet,
        );
        funcItem.insertText = new vscode.SnippetString(
          "${1|#,%,@,$,?|}func ${2:name}(${3:}) {\n\t$0\n}",
        );
        funcItem.detail = "Typed function declaration";

        const switchItem = new vscode.CompletionItem(
          "switch",
          vscode.CompletionItemKind.Snippet,
        );
        switchItem.insertText = new vscode.SnippetString(
          "switch (${1:}) {\n\targ ${2:}:\n\t\t$3\n\t\tend\n\tincase:\n\t\t$4\n\t\tend\n}",
        );
        switchItem.detail = "Switch conditional block";

        const argItem = new vscode.CompletionItem(
          "arg",
          vscode.CompletionItemKind.Snippet,
        );
        argItem.insertText = new vscode.SnippetString(
          "arg ${1:}:\n\t$0\n\tend",
        );
        argItem.detail = "Arg case block";

        const ifItem = new vscode.CompletionItem(
          "if",
          vscode.CompletionItemKind.Snippet,
        );
        ifItem.insertText = new vscode.SnippetString("if ${1:} {\n\t$0\n}");
        ifItem.detail = "If statement block";

        const elifItem = new vscode.CompletionItem(
          "elif",
          vscode.CompletionItemKind.Snippet,
        );
        elifItem.insertText = new vscode.SnippetString("elif ${1:} {\n\t$0\n}");
        elifItem.detail = "Elif statement block";

        const elseItem = new vscode.CompletionItem(
          "else",
          vscode.CompletionItemKind.Snippet,
        );
        elseItem.insertText = new vscode.SnippetString("else {\n\t$0\n}");
        elseItem.detail = "Else block";

        const repeatItem = new vscode.CompletionItem(
          "repeat",
          vscode.CompletionItemKind.Snippet,
        );
        repeatItem.insertText = new vscode.SnippetString(
          "repeat ${1:start}, ${2:condition}, ${3:step} {\n\t$0\n}",
        );
        repeatItem.detail = "Repeat loop block";

        const duringItem = new vscode.CompletionItem(
          "during",
          vscode.CompletionItemKind.Snippet,
        );
        duringItem.insertText = new vscode.SnippetString(
          "during ${1:condition} {\n\t$0\n}",
        );
        duringItem.detail = "During loop block";

        items.push(
          mainItem,
          funcItem,
          switchItem,
          argItem,
          ifItem,
          elifItem,
          elseItem,
          repeatItem,
          duringItem,
        );

        return items;
      },
    },
    ".",
    " ",
    "#",
    "&",
  );

  context.subscriptions.push(completionProvider);

  // --- DIAGNOSTICS LINTER ---
  function validateDocument(document) {
    if (document.languageId !== "night") return;

    const text = document.getText();
    const diagnostics = [];

    const booleanAlias = getLibraryAlias(text, "boolean");
    const terminalAlias = getLibraryAlias(text, "terminal");
    const stringAlias = getLibraryAlias(text, "string");

    const lines = text.split("\n");

    lines.forEach((lineText, lineIdx) => {
      if (lineText.trim().startsWith(";")) return;

      // Validate fixed-width integer sizes (only allow 8, 16, 32, 64)
      const invalidBitMatches = lineText.matchAll(
        /#(\d+)&([a-zA-Z_][a-zA-Z0-9_]*)/g,
      );
      for (const match of invalidBitMatches) {
        const size = match[1];
        if (!["8", "16", "32", "64"].includes(size)) {
          const range = new vscode.Range(
            lineIdx,
            match.index,
            lineIdx,
            match.index + match[0].length,
          );
          diagnostics.push(
            new vscode.Diagnostic(
              range,
              `Invalid fixed integer width '${size}'. Night only supports 8, 16, 32, and 64 bit integers.`,
              vscode.DiagnosticSeverity.Error,
            ),
          );
        }
      }

      // Check boolean imports (Added /g flag to matchAll regex)
      if (!booleanAlias) {
        const boolMatches = lineText.matchAll(/\b(true|false|nil)\b/g);
        for (const match of boolMatches) {
          const range = new vscode.Range(
            lineIdx,
            match.index,
            lineIdx,
            match.index + match[0].length,
          );
          diagnostics.push(
            new vscode.Diagnostic(
              range,
              `Cannot use '${match[0]}' without importing 'using boolean'.`,
              vscode.DiagnosticSeverity.Error,
            ),
          );
        }
      }

      // Check terminal imports and active aliases
      if (terminalAlias) {
        if (terminalAlias !== "terminal") {
          const wrongTerminalMatches = lineText.matchAll(
            /\bterminal\.(out|in|nl)\b/g,
          );
          for (const match of wrongTerminalMatches) {
            const range = new vscode.Range(
              lineIdx,
              match.index,
              lineIdx,
              match.index + match[0].length,
            );
            diagnostics.push(
              new vscode.Diagnostic(
                range,
                `'terminal' is aliased as '${terminalAlias}'. Use '${terminalAlias}.${match[1]}' instead.`,
                vscode.DiagnosticSeverity.Error,
              ),
            );
          }
        }
      } else {
        const termMatches = lineText.matchAll(
          /\b[a-zA-Z_][a-zA-Z0-9_]*\.(out|in|nl)\b/g,
        );
        for (const match of termMatches) {
          const range = new vscode.Range(
            lineIdx,
            match.index,
            lineIdx,
            match.index + match[0].length,
          );
          diagnostics.push(
            new vscode.Diagnostic(
              range,
              `Cannot call terminal method '${match[0]}' without importing 'using terminal'.`,
              vscode.DiagnosticSeverity.Error,
            ),
          );
        }
      }

      // Check string imports
      if (!stringAlias) {
        const stringMatches = lineText.matchAll(
          /\b(length|copy|overwrite|compare|subSearch|slice|sliceArray|join|trim)\b(?=\s*\()/g,
        );
        for (const match of stringMatches) {
          const range = new vscode.Range(
            lineIdx,
            match.index,
            lineIdx,
            match.index + match[0].length,
          );
          diagnostics.push(
            new vscode.Diagnostic(
              range,
              `Cannot use '${match[1]}' without importing 'using string'.`,
              vscode.DiagnosticSeverity.Error,
            ),
          );
        }
      }
    });

    diagnosticsCollection.set(document.uri, diagnostics);
  }

  vscode.workspace.onDidOpenTextDocument(
    validateDocument,
    null,
    context.subscriptions,
  );
  vscode.workspace.onDidChangeTextDocument(
    (e) => validateDocument(e.document),
    null,
    context.subscriptions,
  );
  if (vscode.window.activeTextEditor) {
    validateDocument(vscode.window.activeTextEditor.document);
  }
}

function extendMarkdownIt(md) {
  const defaultFence =
    md.renderer.rules.fence ||
    function (tokens, idx, options, env, self) {
      return self.renderToken(tokens, idx, options);
    };

  md.renderer.rules.fence = (tokens, idx, options, env, self) => {
    const token = tokens[idx];
    const lang = (token.info || "").trim().toLowerCase();

    if (["night", "nightlang", "prognight", "n"].includes(lang)) {
      const highlightedCode = highlightNightCode(token.content);
      return `<pre class="vscode-light vscode-dark"><code class="language-night">${highlightedCode}</code></pre>\n`;
    }

    return defaultFence(tokens, idx, options, env, self);
  };

  return md;
}

function highlightNightCode(code) {
  const patterns = [
    {
      type: "comment",
      regex: /^;-[\s\S]*?-;/,
      style: "color: #6A9955; font-style: italic;",
    },
    {
      type: "comment",
      regex: /^;[^\r\n]*/,
      style: "color: #6A9955; font-style: italic;",
    },
    {
      type: "string",
      regex: /^"(?:[^"\\]|\\.)*"|^'(?:[^'\\]|\\.)*'/,
      style: "color: #CE9178;",
    },
    {
      type: "fixedInt",
      regex: /^(#)(8|16|32|64)(&)([a-zA-Z_][a-zA-Z0-9_]*)/,
      style: "color: #569CD6;",
      sizeStyle: "color: #B5CEA8;",
      ampStyle: "color: #D4D4D4;",
      varStyle: "color: #9CDCFE;",
    },
    {
      type: "keyword",
      regex:
        /^\b(if|elif|else|during|repeat|switch|arg|incase|attempt|send|end|kill|break|continue|main|using|as)\b/,
      style: "color: #C586C0; font-weight: bold;",
    },
    {
      type: "module",
      regex: /^\b(terminal|boolean|string|math|file)\b/,
      style: "color: #4EC9B0;",
    },
    {
      type: "variable",
      regex: /^([#%@$?])([a-zA-Z_][a-zA-Z0-9_]*)/,
      style: "color: #569CD6;",
      subStyle: "color: #9CDCFE;",
    },
    {
      type: "number",
      regex: /^\b\d+(?:\.\d+)?\b/,
      style: "color: #B5CEA8;",
    },
    {
      type: "operator",
      regex: /^(->>>|->>|->|<<<-|<<-|<-|\.)/,
      style: "color: #D4D4D4;",
    },
  ];

  let result = "";
  let pos = 0;

  while (pos < code.length) {
    let matched = false;

    if (
      code[pos] === "\n" ||
      code[pos] === " " ||
      code[pos] === "\t" ||
      code[pos] === "\r"
    ) {
      const char = code[pos];
      result +=
        char === "&"
          ? "&amp;"
          : char === "<"
            ? "&lt;"
            : char === ">"
              ? "&gt;"
              : char;
      pos++;
      continue;
    }

    const rest = code.slice(pos);

    for (const p of patterns) {
      const m = rest.match(p.regex);
      if (m) {
        matched = true;
        const text = m[0];

        if (p.type === "fixedInt" && m[1] && m[2] && m[3] && m[4]) {
          result +=
            `<span style="${p.style}">${escapeHtml(m[1])}</span>` +
            `<span style="${p.sizeStyle}">${escapeHtml(m[2])}</span>` +
            `<span style="${p.ampStyle}">${escapeHtml(m[3])}</span>` +
            `<span style="${p.varStyle}">${escapeHtml(m[4])}</span>`;
        } else if (p.type === "variable" && m[1] && m[2]) {
          result += `<span style="${p.style}">${escapeHtml(m[1])}</span><span style="${p.subStyle}">${escapeHtml(m[2])}</span>`;
        } else {
          result += `<span style="${p.style}">${escapeHtml(text)}</span>`;
        }

        pos += text.length;
        break;
      }
    }

    if (!matched) {
      result += escapeHtml(code[pos]);
      pos++;
    }
  }

  return result;
}

function escapeHtml(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function deactivate() {}

module.exports = {
  activate,
  deactivate,
  extendMarkdownIt,
};
