// Standalone wintrust.dll compatibility shim for Wine/Proton.
// Wine's builtin DLLs in C:\windows\ carry no Microsoft Authenticode signature,
// causing games that verify loaded system modules via WinVerifyTrust to fail
// on startup under Linux/Proton.
//
// This shim reports files under C:\windows\ as trusted with an in-memory
// Microsoft certificate context so CertGetNameStringW succeeds natively.
//
// Build:
//   x86_64-w64-mingw32-gcc -O2 -shared -o wintrust.dll wintrust_shim.c wintrust.def \
//       -nostartfiles -Wl,--entry,DllMain -lkernel32 -lcrypt32

#include <windows.h>
#include <wintrust.h>
#include <wincrypt.h>
#include <softpub.h>
#include <mscat.h>

#define FAKE_STATE_HANDLE ((HANDLE)(ULONG_PTR)0x57545348)

static CRYPT_PROVIDER_DATA g_provData;
static CRYPT_PROVIDER_SGNR g_provSgnr;
static CRYPT_PROVIDER_CERT g_provCert;
static PCCERT_CONTEXT g_fakeCert = NULL;

static void ensure_fake_cert(void) {
    if (g_fakeCert) return;
    DWORD cbEncoded = 0;
    const WCHAR *name = L"CN=Microsoft Windows, O=Microsoft Corporation";
    if (CertStrToNameW(X509_ASN_ENCODING, name, CERT_X500_NAME_STR, NULL, NULL, &cbEncoded, NULL)) {
        BYTE *pbEncoded = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cbEncoded);
        if (pbEncoded) {
            if (CertStrToNameW(X509_ASN_ENCODING, name, CERT_X500_NAME_STR, NULL, pbEncoded, &cbEncoded, NULL)) {
                CERT_NAME_BLOB blob = { cbEncoded, pbEncoded };
                g_fakeCert = CertCreateSelfSignCertificate(
                    0, &blob, CERT_CREATE_SELFSIGN_NO_SIGN, NULL, NULL, NULL, NULL, NULL);
            }
            HeapFree(GetProcessHeap(), 0, pbEncoded);
        }
    }

    ZeroMemory(&g_provCert, sizeof(g_provCert));
    g_provCert.cbStruct = sizeof(g_provCert);
    g_provCert.pCert = g_fakeCert;
    g_provCert.fTrustedRoot = TRUE;
    g_provCert.fSelfSigned = FALSE;

    ZeroMemory(&g_provSgnr, sizeof(g_provSgnr));
    g_provSgnr.cbStruct = sizeof(g_provSgnr);
    g_provSgnr.csCertChain = 1;
    g_provSgnr.pasCertChain = &g_provCert;

    ZeroMemory(&g_provData, sizeof(g_provData));
    g_provData.cbStruct = sizeof(g_provData);
    g_provData.csSigners = 1;
    g_provData.pasSigners = &g_provSgnr;
}

static BOOL is_system_file(WINTRUST_DATA *d) {
    if (!d || d->cbStruct < sizeof(DWORD) * 4) return FALSE;
    if (d->dwUnionChoice != WTD_CHOICE_FILE || !d->pFile || !d->pFile->pcwszFilePath) return FALSE;
    const WCHAR *p = d->pFile->pcwszFilePath;
    if (!_wcsnicmp(p, L"\\\\?\\", 4)) p += 4;
    return _wcsnicmp(p, L"C:\\windows\\", 11) == 0;
}

__declspec(dllexport) LONG WINAPI WinVerifyTrust(HWND hwnd, GUID *action, LPVOID data) {
    WINTRUST_DATA *d = (WINTRUST_DATA *)data;
    if (d) {
        if (d->dwStateAction == WTD_STATEACTION_CLOSE) {
            if (d->hWVTStateData == FAKE_STATE_HANDLE) d->hWVTStateData = NULL;
            return ERROR_SUCCESS;
        }
        if (is_system_file(d)) {
            ensure_fake_cert();
            if (d->dwStateAction == WTD_STATEACTION_VERIFY) {
                d->hWVTStateData = FAKE_STATE_HANDLE;
            }
            return ERROR_SUCCESS;
        }
    }
    return TRUST_E_NOSIGNATURE;
}

__declspec(dllexport) HRESULT WINAPI WinVerifyTrustEx(HWND hwnd, GUID *action, WINTRUST_DATA *data) {
    return WinVerifyTrust(hwnd, action, data);
}

__declspec(dllexport) CRYPT_PROVIDER_DATA *WINAPI WTHelperProvDataFromStateData(HANDLE hStateData) {
    if (hStateData == FAKE_STATE_HANDLE) {
        ensure_fake_cert();
        return &g_provData;
    }
    return NULL;
}

__declspec(dllexport) CRYPT_PROVIDER_SGNR *WINAPI WTHelperGetProvSignerFromChain(
    CRYPT_PROVIDER_DATA *pProvData, DWORD idxSigner, BOOL fCounterSigner, DWORD idxCounterSigner)
{
    if (pProvData == &g_provData && idxSigner == 0 && !fCounterSigner) {
        ensure_fake_cert();
        return &g_provSgnr;
    }
    return NULL;
}

__declspec(dllexport) BOOL WINAPI CryptCATAdminAcquireContext(
    HCATADMIN *phCatAdmin, const GUID *pgSubsystem, DWORD dwFlags)
{
    if (phCatAdmin) *phCatAdmin = (HCATADMIN)(ULONG_PTR)1;
    return TRUE;
}

__declspec(dllexport) BOOL WINAPI CryptCATAdminReleaseContext(HCATADMIN hCatAdmin, DWORD dwFlags) {
    return TRUE;
}

__declspec(dllexport) BOOL WINAPI CryptCATAdminCalcHashFromFileHandle(
    HANDLE hFile, DWORD *pcbHash, BYTE *pbHash, DWORD dwFlags)
{
    if (!pcbHash) return FALSE;
    if (!pbHash || *pcbHash < 20) {
        *pcbHash = 20;
        if (!pbHash) return TRUE;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    ZeroMemory(pbHash, 20);
    *pcbHash = 20;
    return TRUE;
}

__declspec(dllexport) HCATINFO WINAPI CryptCATAdminEnumCatalogFromHash(
    HCATADMIN hCatAdmin, BYTE *pbHash, DWORD cbHash, DWORD dwFlags, HCATINFO *phPrevCatInfo)
{
    SetLastError(ERROR_NOT_FOUND);
    return NULL;
}

__declspec(dllexport) BOOL WINAPI CryptCATCatalogInfoFromContext(
    HCATINFO hCatInfo, CATALOG_INFO *psCatInfo, DWORD dwFlags)
{
    return FALSE;
}

__declspec(dllexport) BOOL WINAPI CryptCATAdminReleaseCatalogContext(
    HCATADMIN hCatAdmin, HCATINFO hCatInfo, DWORD dwFlags)
{
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) {
    return TRUE;
}
