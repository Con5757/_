#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

void diagnosticSetSource(const char *filename, const char *source);
_Noreturn void diagnosticError(const char *phase, const char *code, const char *message,
                     int line, int column, const char *label,
                     const char *help);
_Noreturn void diagnosticErrorWithNote(const char *phase, const char *code,
                             const char *message, int line, int column,
                             const char *label, const char *help,
                             const char *note, int noteLine, int noteColumn);

#endif
