const vscode = require('vscode');

const BUILTINS = {
  'io.out': {
    signature: 'io.out(value)',
    returns: 'void',
    summary: 'Print one value to standard output followed by a newline.',
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
      const lineNumber = text.slice(0, match.index).split('\n').length;
      return {
        signature: match[0].trim(),
        source: uri.fsPath,
        lineNumber
      };
    }
  }
  return null;
}

function activate(context) {
  const provider = vscode.languages.registerHoverProvider('underscore', {
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
  context.subscriptions.push(provider);
}

function deactivate() {}

module.exports = { activate, deactivate };
