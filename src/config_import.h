#pragma once
/* config_import.h — Import chiaki-ng Windows/Linux settings into config.json
 *
 * Call config_try_import_chiaki_ini() early in main(), before config_load().
 * It checks for a chiaki-ng-Default.ini in the app directory and, if found,
 * extracts all registration keys and writes them into config.json.
 *
 * The INI file is renamed to *.imported after a successful import so it will
 * not be re-processed on subsequent launches.
 *
 * The PS5's IP address ("host") is NOT stored in the chiaki-ng INI — the user
 * must still set that field in config.json.  If it was already present in an
 * existing config.json it is automatically preserved.
 */

typedef enum {
    CI_FILE_NOT_FOUND     = 0,  /* No INI present — normal first-run or already imported */
    CI_SUCCESS            = 1,  /* Import OK, host is set */
    CI_SUCCESS_NEEDS_HOST = 2,  /* Import OK, but "host" is blank — user must add PS5 IP */
    CI_PARSE_ERROR        = 3,  /* INI found but required fields missing */
    CI_WRITE_ERROR        = 4,  /* Could not write config.json */
} ChiakiImportResult;

/* Check for chiaki-ng-Default.ini at ini_path and import into config_path.
 * Returns immediately with CI_FILE_NOT_FOUND if no INI is present. */
ChiakiImportResult config_try_import_chiaki_ini(
    const char *ini_path,
    const char *config_path);
