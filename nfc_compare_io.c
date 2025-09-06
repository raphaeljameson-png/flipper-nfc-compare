#include "nfc_compare_io.h"
#include <furi.h>
#include <toolbox/path.h>
#include <dialogs/dialogs.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_device.h>
#include <string.h>

static void zero_profile(NfcProfile* p){ memset(p,0,sizeof(*p)); }

static bool select_file_from_sd(Storage* storage, char* out_path, size_t out_sz){
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    bool ok = dialog_file_browser_show(dialogs, out_path, out_sz, NULL, NULL);
    furi_record_close(RECORD_DIALOGS);
    return ok;
}

static void hash16(const uint8_t* data, size_t len, uint8_t out[16]){
    uint32_t h0=0x12345678, h1=0x9abcdef0, h2=0x0fedcba9, h3=0x87654321;
    for(size_t i=0;i<len;i++){
        h0 = (h0 ^ data[i]) * 2654435761u;
        h1 = (h1 + data[i]) * 2246822519u;
        h2 = (h2 ^ (data[i]<<1)) * 3266489917u;
        h3 = (h3 + (data[i]<<2)) * 668265263u;
    }
    ((uint32_t*)out)[0]=h0; ((uint32_t*)out)[1]=h1; ((uint32_t*)out)[2]=h2; ((uint32_t*)out)[3]=h3;
}

bool load_nfc_profile_from_sd(Storage* storage, NfcProfile* out){
    zero_profile(out);
    char path[256]="";
    if(!select_file_from_sd(storage, path, sizeof(path))) return false;

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) { storage_file_free(f); return false; }

    uint8_t buf[512];
    UINT r=0;
    FRESULT fr = storage_file_read(f, buf, sizeof(buf), &r);
    if(fr!=FR_OK || r==0){ storage_file_close(f); storage_file_free(f); return false; }

    // heuristique simple: extraire UID/ATQA/SAK si texte Flipper, sinon hash du contenu
    // recherche "UID:" dans le texte
    const char* text = (const char*)buf;
    if(memmem(text, r, "UID", 3)){
        // très simplifié: pas robuste, juste démonstratif
        // On copie jusqu'à 10 hex bytes
        size_t u=0;
        for(size_t i=0;i+1<r && u<10;i++){
            if(((text[i]>='0'&&text[i]<='9')||(text[i]>='A'&&text[i]<='F')) &&
               ((text[i+1]>='0'&&text[i+1]<='9')||(text[i+1]>='A'&&text[i+1]<='F'))){
                uint8_t hi = (text[i]>'9'?text[i]-'A'+10:text[i]-'0');
                uint8_t lo = (text[i+1]>'9'?text[i+1]-'A'+10:text[i+1]-'0');
                out->uid[u++] = (hi<<4)|lo;
                i++;
            }
        }
        out->uid_len = u;
    } else {
        // binaire: hash simple
        hash16(buf, r, out->ndef_hash);
        out->has_ndef = true;
    }

    storage_file_close(f);
    storage_file_free(f);
    return true;
}

bool scan_nfc_physical(Nfc* nfc, NfcProfile* out){
    zero_profile(out);
    NfcDevice* dev = nfc_device_alloc();
    bool ok = nfc_poll(nfc, dev, 5000);
    if(!ok){ nfc_device_free(dev); return false; }

    const NfcDeviceCommonData* common = nfc_device_get_data(dev, NfcProtocolUnknown);
    if(common && common->uid_len>0){
        out->uid_len = common->uid_len>16?16:common->uid_len;
        memcpy(out->uid, common->uid, out->uid_len);
    }
    // atqa/sak si dispo
    if(common && common->atqa[0]|common->atqa[1]) {
        out->atqa[0]=common->atqa[0]; out->atqa[1]=common->atqa[1];
        out->sak = common->sak;
    }

    // NDEF, si présent
    const NfcDeviceNdefData* ndef = nfc_device_get_ndef(dev);
    if(ndef && ndef->len>0){
        out->has_ndef = true;
        hash16(ndef->data, ndef->len, out->ndef_hash);
    }

    nfc_device_free(dev);
    return true;
}

int compare_profiles(const NfcProfile* a, const NfcProfile* b){
    if(!a || !b) return -1;
    if(a->uid_len && b->uid_len){
        if(a->uid_len!=b->uid_len) return 1;
        if(memcmp(a->uid, b->uid, a->uid_len)!=0) return 1;
    }
    if(a->has_ndef || b->has_ndef){
        if(!a->has_ndef || !b->has_ndef) return 1;
        if(memcmp(a->ndef_hash, b->ndef_hash, 16)!=0) return 1;
    }
    if(a->atqa[0]!=b->atqa[0] || a->atqa[1]!=b->atqa[1]) return 1;
    if(a->sak!=b->sak) return 1;
    return 0;
}

void free_nfc_profile(NfcProfile* p){
    (void)p;
}
