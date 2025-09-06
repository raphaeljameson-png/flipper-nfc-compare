#pragma once
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <stdbool.h>

typedef struct {
    uint8_t uid[16];
    size_t uid_len;
    uint8_t atqa[2];
    uint8_t sak;
    uint8_t ndef_hash[16];
    bool has_ndef;
} NfcProfile;

bool load_nfc_profile_from_sd(Storage* storage, NfcProfile* out);
bool scan_nfc_physical(Nfc* nfc, NfcProfile* out);
int compare_profiles(const NfcProfile* a, const NfcProfile* b);
void free_nfc_profile(NfcProfile* p);
