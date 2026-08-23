/* odttf.h -- H5: ODTTF embedded-font de-obfuscation (DOCX word/fonts). */
#ifndef WUBUFONT_ODTTF_H
#define WUBUFONT_ODTTF_H

#include <stddef.h>
#include <stdint.h>

/* Parse a GUID string "{...-...}" into the 16-byte ODTTF XOR key
 * (GUID bytes in reversed order). Returns 0 ok, -1 malformed. */
int wubufont_odttf_guid_from_string(const char *guid, uint8_t out[16]);

/* Decode an .odttf blob with the given key. Returns a malloc'd plain sfnt
 * blob (caller frees) and sets *out_len, or NULL. */
uint8_t *wubufont_odttf_decode(const uint8_t *data, size_t len,
                               const uint8_t guid[16], size_t *out_len);

/* 1 if the blob is already an unobfuscated sfnt/ttc. */
int wubufont_odttf_is_plain_sfnt(const uint8_t *data, size_t len);

#endif /* WUBUFONT_ODTTF_H */
