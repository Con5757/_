'use strict';

const vscode = require('vscode');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const BUILTINS = {
  'io.out': {
    signature: 'io.out(value) -> void',
    summary: 'Writes one value to standard output followed by a newline. Strings, integers, floating-point values, booleans, and pointers are formatted by the native runtime.',
    examples: ['io.out("Hello, Underscore!")', 'io.out(42)', 'io.out(3.14159)', 'io.nl()']
  },
  'io.in': {
    signature: 'io.in() -> str',
    summary: 'Reads one line from standard input and returns a runtime-owned string buffer. The buffer is reused by the runtime, so copy the text before making another input call if it must be preserved.',
    examples: ['str name = io.in()', 'io.out(io.in())']
  },
  'io.nl': {
    signature: 'io.nl() -> void',
    summary: 'Writes a newline to standard output.',
    examples: ['io.out("First line")\nio.nl()\nio.out("Second line")']
  },
  'io.errout': {
    signature: 'io.errout(value) -> void',
    summary: 'Writes one formatted value through the runtime output helpers. In this compiler build, it currently uses the same output stream as io.out.',
    examples: ['io.errout("Unable to open file")', 'io.errout(errorCode)']
  },
  'math.pi': {
    signature: 'math.pi() -> fp',
    summary: 'Returns the mathematical constant π.',
    examples: ['fp circumference = 2.0 * math.pi() * radius']
  },
  'math.sqrt': {
    signature: 'math.sqrt(fp value) -> fp',
    summary: 'Returns the square root of a numeric value. The current runtime returns -1 for a negative input.',
    examples: ['fp root = math.sqrt(81.0)', 'io.out(math.sqrt(2.0))']
  },
  'math.abs': {
    signature: 'math.abs(fp value) -> fp',
    summary: 'Returns the absolute value.',
    examples: ['fp distance = math.abs(-4.5)']
  },
  'math.min': {
    signature: 'math.min(fp left, fp right) -> fp',
    summary: 'Returns the smaller of two values.',
    examples: ['fp lower = math.min(3.0, 8.0)']
  },
  'math.max': {
    signature: 'math.max(fp left, fp right) -> fp',
    summary: 'Returns the larger of two values.',
    examples: ['fp higher = math.max(3.0, 8.0)']
  },
  'math.round': {
    signature: 'math.round(fp value) -> fp',
    summary: 'Rounds a value to the nearest whole number.',
    examples: ['fp rounded = math.round(4.6)']
  },
  'math.rdown': {
    signature: 'math.rdown(fp value) -> fp',
    summary: 'Rounds a value down toward negative infinity (floor).',
    examples: ['fp lower = math.rdown(4.9)']
  },
  'math.rup': {
    signature: 'math.rup(fp value) -> fp',
    summary: 'Rounds a value up toward positive infinity (ceiling).',
    examples: ['fp upper = math.rup(4.1)']
  },
  'math.sin': {
    signature: 'math.sin(fp radians) -> fp',
    summary: 'Returns the sine of an angle measured in radians.',
    examples: ['fp y = math.sin(math.pi() / 2.0)']
  },
  'math.cos': {
    signature: 'math.cos(fp radians) -> fp',
    summary: 'Returns the cosine of an angle measured in radians.',
    examples: ['fp x = math.cos(0.0)']
  },
  'math.tan': {
    signature: 'math.tan(fp radians) -> fp',
    summary: 'Returns the tangent of an angle measured in radians.',
    examples: ['fp slope = math.tan(angle)']
  },
  'math.asin': {
    signature: 'math.asin(fp value) -> fp',
    summary: 'Returns the inverse sine in radians.',
    examples: ['fp angle = math.asin(0.5)']
  },
  'math.acos': {
    signature: 'math.acos(fp value) -> fp',
    summary: 'Returns the inverse cosine in radians.',
    examples: ['fp angle = math.acos(0.5)']
  },
  'math.atan': {
    signature: 'math.atan(fp value) -> fp',
    summary: 'Returns the inverse tangent in radians.',
    examples: ['fp angle = math.atan(1.0)']
  },
  'math.log': {
    signature: 'math.log(fp value) -> fp',
    summary: 'Returns the natural logarithm of a positive value.',
    examples: ['fp exponent = math.log(10.0)']
  },
  'math.logx': {
    signature: 'math.logx(fp value, fp base) -> fp',
    summary: 'Returns the logarithm of value in the requested base.',
    examples: ['fp result = math.logx(100.0, 10.0)']
  },
  'math.exp': {
    signature: 'math.exp(fp value) -> fp',
    summary: 'Returns e raised to the supplied power.',
    examples: ['fp growth = math.exp(1.0)']
  },
  'math.sign': {
    signature: 'math.sign(fp value) -> fp',
    summary: 'Returns the sign of a value: negative, zero, or positive.',
    examples: ['fp direction = math.sign(-12.0)']
  },
  'math.clamp': {
    signature: 'math.clamp(fp value, fp low, fp high) -> fp',
    summary: 'Constrains a value to an inclusive lower and upper bound. When low is greater than high, the runtime returns the original value.',
    examples: ['fp volume = math.clamp(input, 0.0, 1.0)']
  },
  'math.lerp': {
    signature: 'math.lerp(fp start, fp end, fp amount) -> fp',
    summary: 'Returns linear interpolation between start and end. The amount is not clamped to the range 0 through 1.',
    examples: ['fp midpoint = math.lerp(10.0, 20.0, 0.5)']
  }
};

const ERROR_GUIDES = {
  E0001: ['The compiler could not open the source file.', 'Check that the file exists, that its path is spelled correctly, and that the current user has permission to read it.', ['underscore main._', 'underscore "src\\main._"']],
  E0002: ['The compiler could not determine the source file length.', 'The file may be inaccessible or the filesystem may have returned an I/O error. Try saving the file again and checking that it is not locked by another process.', ['Save main._, then run: underscore main._']],
  E0003: ['The compiler did not read the complete source file.', 'Check disk access and whether another process is modifying or locking the file. This is an input-file error, not a language syntax error.', ['underscore main._']],
  E0100: ['The lexer found text that is not a valid token.', 'Check the reported character and nearby punctuation. Common causes include a misspelled operator, an unclosed string, or a character that is not part of the language.', ['int total = 2 + 3', 'str message = "Hello"']],
  E1001: ['The parser expected a particular token but found something else.', 'Read the expected token and the token actually reported. Check missing closing parentheses, braces, brackets, commas, colons, or assignment operators.', ['int total = (2 + 3)', 'if (total > 2) {\n    io.out(total)\n}']],
  E1002: ['A declared size or capacity must be a non-negative integer within the supported range.', 'Use a plain whole-number literal for array counts and string-buffer capacities. Do not use a decimal, expression, or negative value for the size.', ['int values<3> = [4, 5, 6]', 'str title[32] = "Chapter One"']],
  E1003: ['The parser expected an expression but could not find one.', 'An expression must produce a value. Check for a missing right-hand operand, an empty initializer, or a function call without a value where one is required.', ['int total = left + right', 'int selected = values[0]', 'io.out(math.sqrt(25.0))']],
  E1004: ['The parser expected a type name.', 'Use a supported primitive or fixed-width type, or declare the relevant record/alias before using it. Check spelling and declaration order.', ['int count = 0', 'fp ratio = 0.5', 'str label[32] = "ready"', 'int32@packetCount = 100']],
  E1005: ['A function flag list was not closed.', 'If a function uses angle-bracket flags, close the list with `>`. Remove a dangling comma or incomplete flag.', ['int add(int a, int b) <void> {\n    retreat a + b\n}']],
  E1006: ['A constant declaration is missing its initializer.', 'Constants must be initialized when declared. Put `= value` on the declaration and do not assign to the constant later.', ['const int MAX = 100', 'const fp TAX_RATE = 0.05']],
  E1007: ['Square-bracket buffer capacity is only valid for a string declaration.', 'Use `[N]` after a `str` variable name. Numeric arrays use angle brackets for their element count.', ['str message[64] = "Hello"', 'int scores<3> = [10, 20, 30]']],
  E1008: ['The array initializer count does not match the declared array size.', 'The number inside `<N>` must equal the number of initializer elements inside `[ ... ]`. Add or remove elements, or correct the declared count.', ['int scores<3> = [10, 20, 30]', 'fp samples<2> = [0.5, 1.5]']],
  E1009: ['A record contains a member that is not a valid field or typed method.', 'Inside a `data` block, declare fields with a type and name, and declare methods with a return type, name, typed parameters, and a body.', ['data Person {\n    str name[32]\n    int age\n    void greet() <void> {\n        io.out("Hello")\n    }\n}']],
  E1010: ['The enum declaration contains invalid punctuation or an unexpected value.', 'Separate enum names with commas or line breaks, and close the declaration with `}`.', ['set {\n    RED,\n    GREEN,\n    BLUE\n}']],
  E1011: ['A switch statement contains more than one default (`incase`) branch.', 'A switch may have only one default branch. Keep one `incase:` and move additional fallback behavior into that branch.', ['switch (choice) {\n    arg 1: io.out("one")\n    incase: io.out("other")\n}']],
  E1012: ['The parser expected a switch case or default label.', 'Inside `switch`, start each branch with `arg value:`. Use at most one `incase:` for the default branch.', ['switch (choice) {\n    arg 1:\n        io.out("one")\n        break\n    incase:\n        io.out("other")\n        break\n}']],
  E1013: ['A string initializer is longer than the declared writable buffer.', 'The capacity counts text bytes and does not include the terminating NUL byte. Increase `[N]` until it is at least the initializer length.', ['str greeting[32] = "Hello, there!"', 'str message[128] = "A longer message"']],
  E1014: ['The function uses a flag that the current compiler does not support.', 'Only `void`, `static`, and `inline` function flags are accepted by this compiler version. Remove the unsupported flag or use a supported one.', ['int twice(int value) <static,inline> {\n    retreat value * 2\n}']],
  E1015: ['The `attempt/incase` exception-handling syntax is not implemented yet.', 'Use supported conditional logic (`if`, `elif`, `else`) for ordinary branching. Do not rely on `attempt` to catch runtime errors.', ['if (valid) {\n    io.out("OK")\n} else {\n    io.out("Not valid")\n}']],
  E2001: ['A variable or declaration name is already in use in this translation unit.', 'Choose a unique name, or remove the duplicate declaration. Check every source file passed to the compiler because all input files are combined into one program.', ['int count = 1\nint total = count + 2']],
  E2002: ['A function has more than one body/definition.', 'A function may have a declaration and one definition, but it cannot have two definitions. Keep one body and place a prototype in a shared source/header convention if needed.', ['int add(int a, int b)\nint add(int a, int b) <void> {\n    retreat a + b\n}']],
  E2003: ['The program calls a function that the compiler cannot resolve.', 'Check the function spelling and its declaration. For native calls, import the module and use an API name supported by the runtime.', ['?std\n    with io, math\n\nvoid main() <void> {\n    io.out(math.sqrt(49.0))\n    kill -silent\n}', 'int result = add(2, 3)']],
  E2004: ['The left side of an assignment is not a writable location.', 'Assign to a variable, array element, record field, or valid dereferenced pointer. A literal or computed expression cannot be assigned to.', ['count = count + 1', 'scores[0] = 99', 'person.age = 30', '<-countPointer = 12']],
  E2005: ['More than one `main` function was found across the compiled source files.', 'Keep exactly one program entry point. Other source files should define helper functions, records, aliases, or globals—not another `main`.', ['; main._\nvoid main() <void> {\n    io.out("Started")\n    kill -silent\n}', '; math_helpers._\nint square(int value) {\n    retreat value * value\n}']],
  E3001: ['A native I/O output call received the wrong number of arguments.', 'Pass exactly one value to `io.out` or `io.errout`. Use `io.nl()` separately when you need a newline.', ['io.out("Hello")', 'io.out(total)\nio.nl()']],
  E4001: ['The compiler could not locate `runtime.c`.', 'Keep `runtime.c` beside the compiler executable or run the compiler from a directory where the runtime can be found. Check the installation layout.', ['compiler/underscore.exe\ncompiler/runtime.c']],
  E4002: ['The compiler could not create its temporary generated C file.', 'Check that the working directory is writable, that antivirus software is not blocking temporary files, and that there is enough disk space.', ['Run the compiler from a writable project/build directory.']],
  E4003: ['The compiler failed while generating C from the parsed program.', 'This is normally an unsupported construct or an internal compiler limitation. Keep the smallest source example that reproduces it for debugging.', ['void main() <void> {\n    io.out("Hello")\n    kill -silent\n}']],
  E4004: ['Clang could not lower the generated C program to LLVM IR.', 'Check that Clang is installed and on PATH. Also inspect any Clang diagnostics printed immediately before this message; they usually identify the generated-code or toolchain issue.', ['clang --version', 'clang -std=c11 -O1 -S -emit-llvm generated.c -o generated.ll']],
  E4005: ['Clang could not compile the C runtime into LLVM IR.', 'Verify that the shipped `runtime.c` matches the compiler build and that the configured Clang toolchain supports C11 and LLVM IR output.', ['clang -std=c11 -O1 -S -emit-llvm runtime.c -o runtime.ll']],
  E4006: ['The final LLVM link step failed.', 'Check the Clang installation, target architecture, and linker output above this message. Remove stale generated files only after confirming that no other build is running.', ['clang program.ll runtime.ll -lm -o program']]
};

const diagnosticCollection = vscode.languages.createDiagnosticCollection('underscore');
const scheduledRuns = new Map();
const runIds = new Map();
const documentUris = new Map();
let nextRunId = 1;
let errorLensDecoration;
let warningLensDecoration;
let outputChannel;

function config() {
  return vscode.workspace.getConfiguration('underscore');
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function resolveCompilerPath() {
  const configured = config().get('compilerPath', '').trim();
  if (configured) return configured;
  const roots = vscode.workspace.workspaceFolders || [];
  const names = process.platform === 'win32'
    ? ['underscore.exe', 'compiler.exe', 'uc.exe', 'underscore']
    : ['underscore', 'compiler', 'uc'];
  const candidates = [];
  for (const root of roots) {
    const base = root.uri.fsPath;
    for (const name of names) {
      candidates.push(
        path.join(base, 'compiler', name),
        path.join(base, 'build', name),
        path.join(base, 'bin', name),
        path.join(base, 'underscore', 'compiler', name),
        path.join(base, name)
      );
    }
  }
  return candidates.find(candidate => {
    try { return fs.statSync(candidate).isFile(); } catch (_) { return false; }
  }) || '';
}

function resolveStdlibPath() {
  const configured = config().get('standardLibraryPath', '').trim();
  if (configured) return configured;
  for (const folder of vscode.workspace.workspaceFolders || []) {
    const root = folder.uri.fsPath;
    const candidates = [
      path.join(root, 'std'),
      path.join(root, 'libraries', 'std'),
      path.join(root, 'underscore', 'std')
    ];
    const result = candidates.find(candidate => {
      try { return fs.statSync(candidate).isDirectory(); } catch (_) { return false; }
    });
    if (result) return result;
  }
  return '';
}

function guideFor(code, message) {
  const guide = ERROR_GUIDES[code];
  if (guide) return { title: guide[0], explanation: guide[1], examples: guide[2] };
  if (/undefined|undeclared|unknown function/i.test(message)) {
    return {
      title: 'The compiler cannot find the referenced function or name.',
      explanation: 'Check spelling, declaration order, source files passed to the compiler, and whether the relevant standard-library module is imported.',
      examples: ['?std\n    with io\n\nvoid main() <void> {\n    io.out("Hello")\n    kill -silent\n}', 'int add(int a, int b) {\n    retreat a + b\n}']
    };
  }
  if (/type|incompatible/i.test(message)) {
    return {
      title: 'The expression or assignment uses an incompatible type.',
      explanation: 'Compare the declared type with the value being assigned or returned. Use an explicit conversion only when the language/runtime provides one.',
      examples: ['int count = 10', 'fp ratio = 0.5', 'str title[32] = "Hello"']
    };
  }
  return {
    title: 'The compiler reported a diagnostic without a specialised guide.',
    explanation: 'Read the compiler message below, then inspect the reported line and nearby declarations. The source message is preserved exactly in the Problems panel.',
    examples: ['void main() <void> {\n    io.out("Hello, Underscore!")\n    kill -silent\n}']
  };
}

function diagnosticMarkdown(diagnostic) {
  const rawCode = diagnostic.code;
  const code = rawCode && typeof rawCode === 'object' ? String(rawCode.value || '') : String(rawCode || '');
  const guide = guideFor(code, diagnostic.message);
  const markdown = new vscode.MarkdownString();
  markdown.isTrusted = false;
  markdown.supportHtml = false;
  markdown.appendMarkdown(`### ${code ? `\`${code}\` — ` : ''}${guide.title}\n\n`);
  markdown.appendMarkdown(`**What to check**\n\n${guide.explanation}\n\n`);
  markdown.appendMarkdown('**Compiler message**\n\n');
  markdown.appendCodeblock(diagnostic.message, 'text');
  markdown.appendMarkdown('**Example patterns**\n\n');
  for (const example of guide.examples) markdown.appendCodeblock(example, 'underscore');
  markdown.appendMarkdown('\n*The examples illustrate the intended syntax; adapt names and types to your program.*');
  return markdown;
}

function makeDiagnostic(line, column, message, severity, code) {
  const safeLine = Math.max(0, line - 1);
  const safeColumn = Math.max(0, column - 1);
  const diagnostic = new vscode.Diagnostic(
    new vscode.Range(safeLine, safeColumn, safeLine, safeColumn + 1),
    message,
    severity
  );
  diagnostic.source = 'Underscore';
  if (code) diagnostic.code = code;
  diagnostic.relatedInformation = [];
  return diagnostic;
}

function parseCompilerOutput(output, sourcePath, tempPath) {
  const lines = output.split(/\r?\n/);
  const parsed = [];
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const header = line.match(/^\s*(error|warning)(?:\[([^\]]+)\])?:\s*(.*)$/i);
    if (!header) continue;

    let location = null;
    for (let j = i + 1; j < Math.min(lines.length, i + 9); j++) {
      const match = lines[j].match(/^\s*-->\s+(.+?):(\d+):(\d+)\s*$/) ||
        lines[j].match(/^\s*at\s+(.+?):(\d+):(\d+)\s*$/);
      if (match) {
        location = { file: match[1], line: Number(match[2]), column: Number(match[3]) };
        break;
      }
      if (/^\s*(?:error|warning)(?:\[[^\]]+\])?:/i.test(lines[j])) break;
    }

    const severity = header[1].toLowerCase() === 'warning'
      ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error;
    const code = header[2] || (severity === vscode.DiagnosticSeverity.Error ? 'E9000' : 'W9000');
    const message = header[3] || code;
    let file = location ? location.file : sourcePath;
    if (path.resolve(file) === path.resolve(tempPath) || path.basename(file) === path.basename(tempPath)) {
      file = sourcePath;
    } else if (!path.isAbsolute(file)) {
      const root = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
        ? vscode.workspace.workspaceFolders[0].uri.fsPath : path.dirname(sourcePath);
      file = path.resolve(root, file);
    }
    parsed.push({
      file,
      diagnostic: makeDiagnostic(location ? location.line : 1, location ? location.column : 1, message, severity, code)
    });
  }
  return parsed;
}

function setDocumentDiagnostics(document, diagnostics) {
  const key = document.uri.toString();
  for (const oldUri of documentUris.get(key) || []) diagnosticCollection.delete(oldUri);
  const grouped = new Map();
  for (const item of diagnostics) {
    const uri = path.resolve(item.file) === path.resolve(document.uri.fsPath)
      ? document.uri : vscode.Uri.file(item.file);
    const uriKey = uri.toString();
    if (!grouped.has(uriKey)) grouped.set(uriKey, { uri, diagnostics: [] });
    grouped.get(uriKey).diagnostics.push(item.diagnostic);
  }
  const updated = [];
  for (const group of grouped.values()) {
    diagnosticCollection.set(group.uri, group.diagnostics);
    updated.push(group.uri);
  }
  if (!grouped.has(document.uri.toString())) {
    diagnosticCollection.set(document.uri, []);
    updated.push(document.uri);
  }
  documentUris.set(key, updated);
  refreshDecorations();
}

function reportCompilerUnavailable(document, message) {
  if (!config().get('enableLiveDiagnostics', true)) return;
  const diagnostic = makeDiagnostic(1, 1, message, vscode.DiagnosticSeverity.Warning, 'W9000');
  setDocumentDiagnostics(document, [{ file: document.uri.fsPath, diagnostic }]);
}

function runCompilerCheck(document, runId, showOutput = false) {
  return new Promise(async resolve => {
    if (document.isClosed || document.uri.scheme !== 'file') return resolve(false);
    const compilerPath = resolveCompilerPath();
    if (!compilerPath) {
      reportCompilerUnavailable(document, 'Compiler not found. Set underscore.compilerPath to the Underscore compiler executable.');
      return resolve(false);
    }

    const sourcePath = document.uri.fsPath;
    const tempPath = path.join(path.dirname(sourcePath), `.__underscore_diag_${process.pid}_${nextRunId++}.u`);
    try {
      await fs.promises.writeFile(tempPath, document.getText(), 'utf8');
    } catch (error) {
      reportCompilerUnavailable(document, `Could not create a temporary diagnostics source file: ${error.message}`);
      return resolve(false);
    }

    const env = { ...process.env };
    const stdlib = resolveStdlibPath();
    if (stdlib) env.UNDERSCORE_STDLIB = stdlib;
    const cwd = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
      ? vscode.workspace.workspaceFolders[0].uri.fsPath : path.dirname(sourcePath);
    const timeoutMs = Math.max(1000, config().get('diagnosticTimeoutMs', 10000));
    const outputLimit = Math.max(16384, config().get('maxDiagnosticOutputChars', 250000));
    let stdout = '';
    let stderr = '';
    let retainedOutputChars = 0;
    let timedOut = false;
    let settled = false;
    let child;
    let timeout;

    const finish = async (success, code = null, processError = null) => {
      if (settled) return;
      settled = true;
      if (timeout) clearTimeout(timeout);
      try { await fs.promises.unlink(tempPath); } catch (_) { /* temporary file cleanup is best-effort */ }
      if (runIds.get(document.uri.toString()) !== runId || document.isClosed) return resolve(false);

      const combined = `${stderr}\n${stdout}`.trim();
      if (showOutput && outputChannel) {
        outputChannel.appendLine(`\n[${path.basename(sourcePath)}]`);
        outputChannel.appendLine(combined || (success ? 'Underscore: check passed' : `Compiler exited with code ${code}`));
      }
      const parsed = parseCompilerOutput(combined, sourcePath, tempPath);
      if (!success && parsed.length === 0) {
        const codeLabel = timedOut ? 'E9001' : 'E9000';
        const message = timedOut
          ? `Compiler check exceeded the ${timeoutMs} ms timeout and was terminated.`
          : (processError ? `Could not start the compiler: ${processError.message}` : `Compiler check failed (exit ${code}). ${combined || 'No diagnostic text was returned.'}`);
        parsed.push({ file: sourcePath, diagnostic: makeDiagnostic(1, 1, message, vscode.DiagnosticSeverity.Error, codeLabel) });
      }
      setDocumentDiagnostics(document, parsed);
      resolve(success);
    };

    try {
      child = spawn(compilerPath, [tempPath, '-check'], { cwd, env, windowsHide: true });
    } catch (error) {
      return finish(false, null, error);
    }
    child.stdout.on('data', chunk => {
      const value = chunk.toString();
      if (retainedOutputChars < outputLimit) {
        const kept = value.slice(0, outputLimit - retainedOutputChars);
        stdout += kept;
        retainedOutputChars += kept.length;
      }
    });
    child.stderr.on('data', chunk => {
      const value = chunk.toString();
      if (retainedOutputChars < outputLimit) {
        const kept = value.slice(0, outputLimit - retainedOutputChars);
        stderr += kept;
        retainedOutputChars += kept.length;
      }
    });
    child.once('error', error => finish(false, null, error));
    child.once('close', code => finish(code === 0, code));
    timeout = setTimeout(() => {
      timedOut = true;
      try { child.kill(); } catch (_) { /* process may already have exited */ }
    }, timeoutMs);
  });
}

function scheduleDiagnostics(document) {
  if (!document || document.uri.scheme !== 'file' || document.languageId !== 'underscore' ||
      path.extname(document.uri.fsPath).toLowerCase() === '.ufg') return;
  const key = document.uri.toString();
  const oldTimer = scheduledRuns.get(key);
  if (oldTimer) clearTimeout(oldTimer);
  const runId = nextRunId++;
  runIds.set(key, runId);

  if (!config().get('enableLiveDiagnostics', true)) {
    for (const uri of documentUris.get(key) || [document.uri]) diagnosticCollection.delete(uri);
    documentUris.set(key, []);
    refreshDecorations();
    return;
  }

  const delay = Math.max(100, config().get('diagnosticDelayMs', 450));
  scheduledRuns.set(key, setTimeout(() => {
    scheduledRuns.delete(key);
    runCompilerCheck(document, runId).catch(error => {
      reportCompilerUnavailable(document, `Underscore diagnostics failed: ${error.message}`);
    });
  }, delay));
}

function refreshDecorations() {
  if (!errorLensDecoration || !warningLensDecoration) return;
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== 'underscore' || !config().get('enableInlineErrorLens', true)) {
    if (editor) {
      editor.setDecorations(errorLensDecoration, []);
      editor.setDecorations(warningLensDecoration, []);
    }
    return;
  }

  const diagnostics = diagnosticCollection.get(editor.document.uri) || [];
  const byLine = new Map();
  for (const diagnostic of diagnostics) {
    const line = Math.max(0, Math.min(diagnostic.range.start.line, editor.document.lineCount - 1));
    if (!byLine.has(line)) byLine.set(line, []);
    byLine.get(line).push(diagnostic);
  }

  const errors = [];
  const warnings = [];
  const maxMessages = Math.max(1, config().get('inlineErrorLensMaxMessages', 2));
  const maxLength = Math.max(60, config().get('inlineErrorLensMaxLength', 150));
  for (const [line, items] of byLine) {
    items.sort((a, b) => (a.severity - b.severity) || a.range.start.character - b.range.start.character);
    const end = editor.document.lineAt(line).text.length;
    const seen = new Set();
    const unique = items.filter(item => {
      const identity = `${item.severity}|${item.code}|${item.message}`;
      if (seen.has(identity)) return false;
      seen.add(identity);
      return true;
    });
    const visible = unique.slice(0, maxMessages).map(item => {
      const code = item.code && typeof item.code === 'object' ? item.code.value : item.code;
      const prefix = item.severity === vscode.DiagnosticSeverity.Warning ? '⚠' : '✕';
      const guide = guideFor(String(code || ''), item.message);
      const compact = `${guide.title} — ${String(item.message).replace(/\s+/g, ' ').trim()}`;
      const budget = Math.max(20, Math.floor(maxLength / maxMessages) - String(code || '').length - 5);
      const summary = compact.length > budget ? `${compact.slice(0, budget - 1)}…` : compact;
      return `${prefix} ${code ? `${code} · ` : ''}${summary}`;
    });
    if (unique.length > maxMessages) visible.push(`+${unique.length - maxMessages} more`);
    let text = `  ⟵ ${visible.join('  |  ')}`;
    if (text.length > maxLength) text = `${text.slice(0, maxLength - 1)}…`;

    const hasError = unique.some(item => item.severity === vscode.DiagnosticSeverity.Error);
    const hover = new vscode.MarkdownString();
    hover.isTrusted = false;
    hover.supportHtml = false;
    unique.forEach((item, index) => {
      if (index) hover.appendMarkdown('\n\n---\n\n');
      hover.appendMarkdown(`**${item.severity === vscode.DiagnosticSeverity.Warning ? 'Warning' : 'Error'}${item.code ? ` · ${String(item.code.value || item.code)}` : ''}**\n\n`);
      hover.appendMarkdown(diagnosticMarkdown(item).value);
    });
    const decoration = {
      range: new vscode.Range(line, end, line, end),
      renderOptions: { after: { contentText: text, margin: '0 0 0 1em' } },
      hoverMessage: hover
    };
    if (hasError) errors.push(decoration);
    else warnings.push(decoration);
  }
  editor.setDecorations(errorLensDecoration, errors);
  editor.setDecorations(warningLensDecoration, warnings);
}

function findQualifiedName(document, position, wordRange) {
  const line = document.lineAt(position.line).text;
  const word = document.getText(wordRange);
  const before = line.slice(0, wordRange.start.character);
  const after = line.slice(wordRange.end.character);
  const qualifier = before.match(/([A-Za-z_]\w*(?:\.|::|~>))\s*$/);
  if (qualifier) return qualifier[1] + word;
  const suffix = after.match(/^\s*(::|\.|~>)\s*([A-Za-z_]\w*)/);
  return suffix ? word + suffix[1] + suffix[2] : word;
}

function findUserFunction(document, qualifiedName) {
  const source = document.getText();
  const baseName = qualifiedName.split(/\.|::|~>/).pop();
  const escaped = escapeRegExp(baseName);
  const type = '(?:int|fp|str|void|bool|int8|int16|int32|int64|fp8|fp16|fp32|fp64|str8|str16|str32|str64|[A-Za-z_]\\w*)';
  const declaration = new RegExp(
    '(^|[;\\n{}])\\s*((?:(?:const|static|global|public|private|hidden)\\s+)*' +
    type + '\\s+(?:->|-->)*\\s*' + escaped + '\\s*\\([^)]*\\)\\s*(?:<[^>]*>)?)',
    'm'
  );
  const match = declaration.exec(source);
  if (!match) return null;
  const signature = match[2].trim();
  const offset = match.index + match[1].length;
  const lineNumber = source.slice(0, offset).split('\n').length;
  const docs = [];
  for (let line = lineNumber - 2; line >= 0; line--) {
    const value = document.lineAt(line).text.trim();
    if (value.startsWith(';') && !value.startsWith(';-')) docs.unshift(value.replace(/^;+\s?/, ''));
    else if (value === '') continue;
    else break;
  }
  return { signature, lineNumber, docs: docs.join('\n') };
}

async function findManifestFunction(name) {
  if (!vscode.workspace.workspaceFolders) return null;
  const baseName = name.split(/\.|::|~>/).pop();
  const escaped = escapeRegExp(baseName);
  const type = '(?:int|fp|str|void|bool|int8|int16|int32|int64|fp8|fp16|fp32|fp64|str8|str16|str32|str64)';
  const declaration = new RegExp('^\\s*(?:(?:const|static|global|public|private|hidden)\\s+)*' + type + '\\s+' + escaped + '\\s*\\([^)]*\\)', 'm');
  const files = await vscode.workspace.findFiles('**/*.ufg', '{**/node_modules/**,**/.git/**,**/build/**}', 100);
  for (const uri of files) {
    try {
      const document = await vscode.workspace.openTextDocument(uri);
      const match = declaration.exec(document.getText());
      if (!match) continue;
      const lineNumber = document.getText().slice(0, match.index).split('\n').length;
      return { signature: match[0].trim(), source: uri.fsPath, lineNumber };
    } catch (_) {
      // Ignore manifests that cannot be opened; continue searching other package manifests.
    }
  }
  return null;
}

function activate(context) {
  outputChannel = vscode.window.createOutputChannel('Underscore');
  errorLensDecoration = vscode.window.createTextEditorDecorationType({
    after: {
      color: new vscode.ThemeColor('editorError.foreground'),
      backgroundColor: new vscode.ThemeColor('inputValidation.errorBackground'),
      border: '1px solid',
      borderColor: new vscode.ThemeColor('inputValidation.errorBorder'),
      margin: '0 0 0 1em',
      fontStyle: 'italic'
    },
    rangeBehavior: vscode.DecorationRangeBehavior.ClosedClosed
  });
  warningLensDecoration = vscode.window.createTextEditorDecorationType({
    after: {
      color: new vscode.ThemeColor('editorWarning.foreground'),
      backgroundColor: new vscode.ThemeColor('inputValidation.warningBackground'),
      border: '1px solid',
      borderColor: new vscode.ThemeColor('editorWarning.foreground'),
      margin: '0 0 0 1em',
      fontStyle: 'italic'
    },
    rangeBehavior: vscode.DecorationRangeBehavior.ClosedClosed
  });
  context.subscriptions.push(outputChannel, errorLensDecoration, warningLensDecoration, diagnosticCollection);

  const hoverProvider = vscode.languages.registerHoverProvider('underscore', {
    async provideHover(document, position) {
      const wordRange = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
      if (!wordRange) return undefined;
      const name = findQualifiedName(document, position, wordRange);
      const builtin = BUILTINS[name];
      if (builtin) {
        const markdown = new vscode.MarkdownString();
        markdown.isTrusted = false;
        markdown.appendCodeblock(builtin.signature, 'underscore');
        markdown.appendMarkdown(`${builtin.summary}\n\n**Examples**\n`);
        builtin.examples.forEach(example => markdown.appendCodeblock(example, 'underscore'));
        return new vscode.Hover(markdown, wordRange);
      }

      const local = findUserFunction(document, name);
      if (local) {
        const markdown = new vscode.MarkdownString();
        markdown.isTrusted = false;
        markdown.appendCodeblock(local.signature, 'underscore');
        markdown.appendMarkdown(local.docs ? `\n${local.docs}` : 'Function declared in this source document.');
        return new vscode.Hover(markdown, wordRange);
      }

      const manifest = await findManifestFunction(name);
      if (!manifest) return undefined;
      const markdown = new vscode.MarkdownString();
      markdown.isTrusted = false;
      markdown.appendCodeblock(manifest.signature, 'underscore');
      markdown.appendMarkdown(`Declared in package manifest: \`${manifest.source}\` (line ${manifest.lineNumber}).`);
      return new vscode.Hover(markdown, wordRange);
    }
  });
  context.subscriptions.push(hoverProvider);

  context.subscriptions.push(vscode.commands.registerCommand('underscore.checkCurrentFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'underscore') {
      vscode.window.showInformationMessage('Open an Underscore source file to run a check.');
      return;
    }
    const key = editor.document.uri.toString();
    const prior = scheduledRuns.get(key);
    if (prior) clearTimeout(prior);
    scheduledRuns.delete(key);
    const runId = nextRunId++;
    runIds.set(key, runId);
    outputChannel.clear();
    outputChannel.show(true);
    const success = await runCompilerCheck(editor.document, runId, true);
    if (success) vscode.window.setStatusBarMessage('Underscore: check passed', 3000);
  }));

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
  for (const key of runIds.keys()) runIds.set(key, nextRunId++);
}

module.exports = { activate, deactivate };
