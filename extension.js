const vscode = require('vscode');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const BUILTINS = {
  'io.out': {
    signature: 'io.out(value)',
    returns: 'void',
    summary: 'Print one value to standard output followed by a newline. This is currently implemented by the native C runtime, not by an ordinary overloaded function body in io._.',
    examples: ['io.out("Hello, Underscore!")', 'io.out(42)']
  },
  'io.in': {
    signature: 'io.in()',
    returns: 'str',
    summary: 'Read one line from standard input. The returned buffer is owned by the IO runtime and is overwritten by the next call.',
    examples: ['str answer = io.in()', 'io.out(io.in())']
  },
  'math.sqrt': {
    signature: 'math.sqrt(fp value)',
    returns: 'fp',
    summary: 'Return an approximate square root. The current source implementation returns -1.0 for negative input.',
    examples: ['fp root = math.sqrt(81.0)', 'io.out(math.sqrt(5.0))']
  },
  'math.abs': {
    signature: 'math.abs(fp value)',
    returns: 'fp',
    summary: 'Return the absolute value of a numeric argument.',
    examples: ['fp distance = math.abs(-4.5)', 'io.out(math.abs(-9.0))']
  },
  'math.min': {
    signature: 'math.min(fp left, fp right)',
    returns: 'fp',
    summary: 'Return the smaller of two numeric values.',
    examples: ['fp smaller = math.min(3.0, 8.0)', 'io.out(math.min(-2.0, 5.0))']
  },
  'math.max': {
    signature: 'math.max(fp left, fp right)',
    returns: 'fp',
    summary: 'Return the larger of two numeric values.',
    examples: ['fp larger = math.max(3.0, 8.0)', 'io.out(math.max(-2.0, 5.0))']
  }
};

function escapeRegExp(text) {
  return text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function findQualifiedName(document, position, wordRange) {
  const line = document.lineAt(position.line).text;
  const word = document.getText(wordRange);
  const before = line.slice(0, wordRange.start.character);
  const after = line.slice(wordRange.end.character);
  const qualifier = before.match(/([A-Za-z_]\w*(?:\.|::))\s*$/);
  if (qualifier) return qualifier[1] + word;
  const suffix = after.match(/^\s*(::|\.)\s*([A-Za-z_]\w*)/);
  if (suffix) return word + suffix[1] + suffix[2];
  return word;
}

function findUserFunction(document, name) {
  const source = document.getText();
  const baseName = name.includes('::') ? name.split('::').pop() : name.split('.').pop();
  const escapedName = escapeRegExp(baseName);
  const declaration = new RegExp(
    '(^|[;\\n{}])\\s*((?:(?:public|private|hidden|static|global|const)\\s+)*' +
    '(?:int|fp|str|void)(?:8|16|32|64)?\\s+(?:func\\s+)?' + escapedName +
    '\\s*\\([^)]*\\)\\s*(?:<[^>]*>)?)',
    'm'
  );
  const match = declaration.exec(source);
  if (!match) return null;
  const signature = match[2].trim().replace(/\s*\{?\s*$/, '');
  const declarationOffset = match.index + match[1].length;
  const lineNumber = source.slice(0, declarationOffset).split('\n').length - 1;
  const leadingLines = [];
  for (let line = lineNumber - 1; line >= 0; --line) {
    const value = document.lineAt(line).text.trim();
    if (value.startsWith(';') && !value.startsWith(';-')) {
      leadingLines.unshift(value.replace(/^;+\s?/, ''));
      continue;
    }
    if (value === '') continue;
    break;
  }
  return { signature, lineNumber, docs: leadingLines.join('\n') };
}

async function findManifestFunction(name) {
  if (!vscode.workspace.workspaceFolders) return null;
  const baseName = name.includes('::') ? name.split('::').pop() : name.split('.').pop();
  const escapedName = escapeRegExp(baseName);
  const declarations = new RegExp(
    '^\\s*(?:(?:global|static|const|public|private|hidden)\\s+)*(?:int|fp|str|void)(?:8|16|32|64)?\\s+' +
    escapedName + '\\s*\\([^)]*\\)(?:\\s*<[^>]*>)?',
    'm'
  );
  const files = await vscode.workspace.findFiles('**/*.ufg', '**/node_modules/**', 100);
  for (const uri of files) {
    let manifest;
    try {
      manifest = await vscode.workspace.openTextDocument(uri);
    } catch (_) {
      continue;
    }
    const text = manifest.getText();
    const match = declarations.exec(text);
    if (match) {
      const lineNumber = text.slice(0, match.index).split('\n').length - 1;
      return { signature: match[0].trim(), source: uri.fsPath, lineNumber };
    }
  }
  return null;
}

const diagnosticCollection = vscode.languages.createDiagnosticCollection('underscore');
const scheduledRuns = new Map();
const runIds = new Map();
const documentUris = new Map();
let nextRunId = 1;
let errorLensDecoration;
let warningLensDecoration;

function configuration() {
  return vscode.workspace.getConfiguration('underscore');
}

function resolveCompilerPath() {
  const configuredPath = configuration().get('compilerPath', '').trim();
  if (configuredPath) return configuredPath;

  const folders = vscode.workspace.workspaceFolders || [];
  const candidates = [];
  for (const folder of folders) {
    const root = folder.uri.fsPath;
    candidates.push(
      path.join(root, 'underscore', 'compiler', 'compiler.exe'),
      path.join(root, 'underscore', 'compiler', 'compiler'),
      path.join(root, 'underscore', 'compiler', 'uc.exe'),
      path.join(root, 'compiler', 'compiler.exe'),
      path.join(root, 'compiler', 'compiler'),
      path.join(root, 'compiler', 'uc.exe')
    );
  }
  return candidates.find(candidate => {
    try { return fs.statSync(candidate).isFile(); } catch (_) { return false; }
  }) || '';
}

function resolveStdlibPath() {
  const configuredPath = configuration().get('standardLibraryPath', '').trim();
  if (configuredPath) return configuredPath;
  const folders = vscode.workspace.workspaceFolders || [];
  for (const folder of folders) {
    const candidates = [
      path.join(folder.uri.fsPath, 'underscore', 'std'),
      path.join(folder.uri.fsPath, 'std')
    ];
    const found = candidates.find(candidate => {
      try { return fs.statSync(candidate).isDirectory(); } catch (_) { return false; }
    });
    if (found) return found;
  }
  return '';
}

function makeDiagnostic(line, column, message, severity, code) {
  const safeLine = Math.max(0, line - 1);
  const safeColumn = Math.max(0, column - 1);
  const range = new vscode.Range(safeLine, safeColumn, safeLine, safeColumn + 1);
  const diagnostic = new vscode.Diagnostic(range, message, severity);
  diagnostic.source = 'Underscore';
  if (code) diagnostic.code = code;
  return diagnostic;
}

function parseCompilerOutput(output, sourcePath, tempPath) {
  const lines = output.split(/\r?\n/);
  const parsed = [];
  for (let i = 0; i < lines.length; i++) {
    const header = lines[i].match(/^\s*(error|warning)\[([^\]]+)\]:\s*(.*)$/i);
    const importHeader = lines[i].match(/^\s*import error:\s*(.*)$/i);
    if (!header && !importHeader) continue;
    let location = null;
    for (let j = i + 1; j < Math.min(lines.length, i + 7); j++) {
      const match = lines[j].match(/^\s*-->\s+(.+):(\d+):(\d+)\s*$/);
      if (match) {
        location = { file: match[1], line: Number(match[2]), column: Number(match[3]) };
        break;
      }
      if (/^\s*(?:error|warning)\[/.test(lines[j])) break;
    }
    if (!location) {
      location = { file: sourcePath, line: 1, column: 1 };
    }
    let filename = location.file;
    if (path.resolve(filename) === path.resolve(tempPath) || path.basename(filename) === path.basename(tempPath)) {
      filename = sourcePath;
    } else if (!path.isAbsolute(filename)) {
      const workspaceRoot = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
        ? vscode.workspace.workspaceFolders[0].uri.fsPath : path.dirname(sourcePath);
      filename = path.resolve(workspaceRoot, filename);
    }
    const severity = header && header[1].toLowerCase() === 'warning'
      ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error;
    const message = header ? (header[3] || header[2]) : `import error: ${importHeader[1]}`;
    const code = header ? header[2] : 'E9001';
    parsed.push({
      file: filename,
      diagnostic: makeDiagnostic(location.line, location.column, message, severity, code)
    });
  }
  return parsed;
}

function reportCompilerUnavailable(document, message) {
  if (!configuration().get('enableLiveDiagnostics', true)) return;
  const key = document.uri.toString();
  for (const uri of documentUris.get(key) || [document.uri]) diagnosticCollection.delete(uri);
  const diagnostic = makeDiagnostic(1, 1, message, vscode.DiagnosticSeverity.Warning, 'W9000');
  diagnosticCollection.set(document.uri, [diagnostic]);
  documentUris.set(key, [document.uri]);
  refreshDecorations();
}

async function runDiagnostics(document, runId) {
  if (document.isClosed || document.uri.scheme !== 'file' || !configuration().get('enableLiveDiagnostics', true)) return;
  const compilerPath = resolveCompilerPath();
  if (!compilerPath) {
    reportCompilerUnavailable(document,
      'Set underscore.compilerPath to a built Underscore compiler to enable live diagnostics.');
    return;
  }

  const sourcePath = document.uri.fsPath;
  const tempPath = path.join(path.dirname(sourcePath),
    `.__underscore_diag_${process.pid}_${nextRunId++}.u`);
  const previousUris = documentUris.get(document.uri.toString()) || [document.uri];
  for (const uri of previousUris) diagnosticCollection.delete(uri);
  refreshDecorations();

  try {
    await fs.promises.writeFile(tempPath, document.getText(), 'utf8');
  } catch (error) {
    reportCompilerUnavailable(document, `Underscore diagnostics could not write a temporary source file: ${error.message}`);
    return;
  }

  const environment = { ...process.env };
  const stdlibPath = resolveStdlibPath();
  if (stdlibPath) environment.UNDERSCORE_STDLIB = stdlibPath;

  const cwd = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
    ? vscode.workspace.workspaceFolders[0].uri.fsPath : path.dirname(sourcePath);

  let stdout = '';
  let stderr = '';
  let launchFailed = false;
  let child;
  try {
    child = spawn(compilerPath, [tempPath, '-check'], { cwd, env: environment, windowsHide: true });
  } catch (error) {
    try { await fs.promises.unlink(tempPath); } catch (_) { /* best effort */ }
    reportCompilerUnavailable(document, `Could not start the Underscore compiler: ${error.message}`);
    return;
  }

  child.stdout.on('data', chunk => { stdout += chunk.toString(); });
  child.stderr.on('data', chunk => { stderr += chunk.toString(); });
  child.on('error', async error => {
    launchFailed = true;
    try { await fs.promises.unlink(tempPath); } catch (_) { /* best effort */ }
    if (runIds.get(document.uri.toString()) === runId) {
      reportCompilerUnavailable(document, `Could not run the Underscore compiler: ${error.message}`);
    }
  });
  child.on('close', async code => {
    try { await fs.promises.unlink(tempPath); } catch (_) { /* best effort */ }
    if (launchFailed || runIds.get(document.uri.toString()) !== runId || document.isClosed) return;

    const output = `${stderr}\n${stdout}`.trim();
    const results = parseCompilerOutput(output, sourcePath, tempPath);
    if (code !== 0 && results.length === 0) {
      results.push({
        file: sourcePath,
        diagnostic: makeDiagnostic(1, 1,
          `Underscore check failed (exit ${code}). ${output || 'No diagnostic text was returned.'}`,
          vscode.DiagnosticSeverity.Error, 'E9000')
      });
    }

    const grouped = new Map();
    for (const result of results) {
      let uri;
      if (path.resolve(result.file) === path.resolve(sourcePath)) uri = document.uri;
      else uri = vscode.Uri.file(result.file);
      const key = uri.toString();
      if (!grouped.has(key)) grouped.set(key, { uri, diagnostics: [] });
      grouped.get(key).diagnostics.push(result.diagnostic);
    }

    const affectedUris = [];
    for (const group of grouped.values()) {
      diagnosticCollection.set(group.uri, group.diagnostics);
      affectedUris.push(group.uri);
    }
    if (!affectedUris.some(uri => uri.toString() === document.uri.toString())) {
      diagnosticCollection.set(document.uri, []);
      affectedUris.push(document.uri);
    }
    documentUris.set(document.uri.toString(), affectedUris);
    refreshDecorations();
  });
}

function scheduleDiagnostics(document) {
  if (document.uri.scheme !== 'file' || document.languageId !== 'underscore' ||
      path.extname(document.uri.fsPath).toLowerCase() === '.ufg') return;
  const key = document.uri.toString();
  const priorTimer = scheduledRuns.get(key);
  if (priorTimer) clearTimeout(priorTimer);
  const runId = nextRunId++;
  runIds.set(key, runId);
  if (!configuration().get('enableLiveDiagnostics', true)) {
    for (const uri of documentUris.get(key) || [document.uri]) diagnosticCollection.delete(uri);
    documentUris.set(key, []);
    refreshDecorations();
    return;
  }
  const delay = Math.max(150, configuration().get('diagnosticDelayMs', 500));
  scheduledRuns.set(key, setTimeout(() => {
    scheduledRuns.delete(key);
    runDiagnostics(document, runId).catch(error => {
      reportCompilerUnavailable(document, `Underscore diagnostics failed: ${error.message}`);
    });
  }, delay));
}

function refreshDecorations() {
  if (!errorLensDecoration || !warningLensDecoration) return;
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== 'underscore' ||
      !configuration().get('enableInlineErrorLens', true)) {
    if (editor) {
      editor.setDecorations(errorLensDecoration, []);
      editor.setDecorations(warningLensDecoration, []);
    }
    return;
  }
  const diagnostics = diagnosticCollection.get(editor.document.uri) || [];
  const errors = [];
  const warnings = [];
  const diagnosticsByLine = new Map();
  for (const diagnostic of diagnostics) {
    const line = Math.max(0, Math.min(diagnostic.range.start.line, editor.document.lineCount - 1));
    if (!diagnosticsByLine.has(line)) diagnosticsByLine.set(line, []);
    diagnosticsByLine.get(line).push(diagnostic);
  }

  for (const [line, lineDiagnostics] of diagnosticsByLine) {
    lineDiagnostics.sort((left, right) => left.range.start.character - right.range.start.character);
    const end = editor.document.lineAt(line).text.length;
    const range = new vscode.Range(line, end, line, end);
    const messages = lineDiagnostics.map(diagnostic => {
      const severity = diagnostic.severity === vscode.DiagnosticSeverity.Warning ? 'warning' : 'error';
      const rawCode = diagnostic.code;
      const code = rawCode && typeof rawCode === 'object' && rawCode.value
        ? String(rawCode.value) : (rawCode === undefined || rawCode === null ? '' : String(rawCode));
      const message = String(diagnostic.message).replace(/\s+/g, ' ').trim();
      return `${severity}${code ? ` [${code}]` : ''}: ${message}`;
    });
    const visibleMessages = messages.slice(0, 2);
    if (messages.length > 2) visibleMessages.push(`+${messages.length - 2} more`);
    let inlineMessage = `  ⟵ ${visibleMessages.join(' · ')}`;
    if (inlineMessage.length > 180) inlineMessage = `${inlineMessage.slice(0, 177)}...`;
    const decoration = {
      range,
      renderOptions: {
        after: { contentText: inlineMessage, margin: '0 0 0 1.2em' }
      },
      hoverMessage: messages.join('\n')
    };
    // Emit one decoration per line. Separate error and warning decorations at the
    // same position overlap, so error styling wins when a line has mixed severities.
    if (lineDiagnostics.some(diagnostic => diagnostic.severity !== vscode.DiagnosticSeverity.Warning)) {
      errors.push(decoration);
    } else {
      warnings.push(decoration);
    }
  }
  editor.setDecorations(errorLensDecoration, errors);
  editor.setDecorations(warningLensDecoration, warnings);
}

function activate(context) {
  errorLensDecoration = vscode.window.createTextEditorDecorationType({
    after: {
      color: new vscode.ThemeColor('editorError.foreground'),
      backgroundColor: '#3e2020',
      border: '1px solid #8b3030',
      margin: '0 0 0 1em',
      fontStyle: 'italic'
    },
    rangeBehavior: vscode.DecorationRangeBehavior.ClosedClosed
  });
  warningLensDecoration = vscode.window.createTextEditorDecorationType({
    after: {
      color: new vscode.ThemeColor('editorWarning.foreground'),
      backgroundColor: '#40351d',
      border: '1px solid #8a6d22',
      margin: '0 0 0 1em',
      fontStyle: 'italic'
    },
    rangeBehavior: vscode.DecorationRangeBehavior.ClosedClosed
  });
  context.subscriptions.push(errorLensDecoration, warningLensDecoration, diagnosticCollection);

  const hoverProvider = vscode.languages.registerHoverProvider('underscore', {
    async provideHover(document, position) {
      const wordRange = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
      if (!wordRange) return undefined;
      const name = findQualifiedName(document, position, wordRange);
      const builtin = BUILTINS[name];
      if (builtin) {
        const markdown = new vscode.MarkdownString();
        markdown.appendCodeblock(`${builtin.signature} -> ${builtin.returns}`, 'underscore');
        markdown.appendMarkdown(`${builtin.summary}\n\n**Examples**\n`);
        for (const example of builtin.examples) markdown.appendCodeblock(example, 'underscore');
        return new vscode.Hover(markdown, wordRange);
      }

      const userFunction = findUserFunction(document, name);
      if (userFunction) {
        const markdown = new vscode.MarkdownString();
        markdown.appendCodeblock(userFunction.signature, 'underscore');
        if (userFunction.docs) markdown.appendMarkdown(`\n${userFunction.docs}`);
        else markdown.appendMarkdown('Function declared in the current source document.');
        return new vscode.Hover(markdown, wordRange);
      }

      const manifestFunction = await findManifestFunction(name);
      if (!manifestFunction) return undefined;
      const markdown = new vscode.MarkdownString();
      markdown.appendCodeblock(manifestFunction.signature, 'underscore');
      markdown.appendMarkdown(`Declared in UFG manifest: \`${manifestFunction.source}\` (line ${manifestFunction.lineNumber}).`);
      return new vscode.Hover(markdown, wordRange);
    }
  });
  context.subscriptions.push(hoverProvider);

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument(scheduleDiagnostics),
    vscode.workspace.onDidChangeTextDocument(event => scheduleDiagnostics(event.document)),
    vscode.workspace.onDidSaveTextDocument(scheduleDiagnostics),
    vscode.window.onDidChangeActiveTextEditor(refreshDecorations),
    vscode.workspace.onDidChangeConfiguration(event => {
      if (event.affectsConfiguration('underscore')) {
        for (const document of vscode.workspace.textDocuments) scheduleDiagnostics(document);
        refreshDecorations();
      }
    }),
    vscode.workspace.onDidCloseTextDocument(document => {
      const key = document.uri.toString();
      const timer = scheduledRuns.get(key);
      if (timer) clearTimeout(timer);
      scheduledRuns.delete(key);
      runIds.set(key, nextRunId++);
      for (const uri of documentUris.get(key) || [document.uri]) diagnosticCollection.delete(uri);
      documentUris.delete(key);
      refreshDecorations();
    })
  );

  for (const document of vscode.workspace.textDocuments) scheduleDiagnostics(document);
}

function deactivate() {
  for (const timer of scheduledRuns.values()) clearTimeout(timer);
  scheduledRuns.clear();
}

module.exports = { activate, deactivate };
