#ifndef UNDERSCORE_CONFIG_MANIFEST_H
#define UNDERSCORE_CONFIG_MANIFEST_H

/*
 * Create a .ufg manifest from the source files in a directory.
 *
 * pathMode == 0: create <source-basename>.ufg beside sourceFile.
 * pathMode != 0: targetDirectory must exist; create
 *                <targetDirectory>/<target-directory-basename>.ufg.
 *
 * Returns 0 on success and non-zero on failure. The function only writes the
 * requested manifest; it does not compile or modify source files.
 */
int manifestCreate(const char *sourceFile, const char *targetDirectory,
                   int pathMode);

#endif
