#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wchar.h>

//extern "C"
BOOL isLnkFileW(WCHAR *str)
{
	str = wcsrchr(str, L'.');
	if (str != NULL)
		return wcscmp(str, L".LNK") == 0 || wcscmp(str, L".lnk") == 0;
//		return wcslen(str) == 4 && towlower(str[1]) == L'l';
	return FALSE;
}

//extern "C"
BOOL resolveLnkW(LPWSTR lpszLinkFile, LPWSTR lpszPath)
{
    HRESULT hr;
    BOOL ok = FALSE;
    IShellLinkW * psl;
    WCHAR szGotPath[MAX_PATH];
    WCHAR szDescription[MAX_PATH];
    WIN32_FIND_DATAW wfd;
    *lpszPath = 0;

    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize has already been called.
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
    if (SUCCEEDED(hr))
    {
        IPersistFile* ppf;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr))
        {
            // Load the shortcut.
            hr = ppf->Load(lpszLinkFile, STGM_READ);
            if (SUCCEEDED(hr))
            {
                // Resolve the link.
                hr = psl->Resolve(NULL, 0); // HWND
                if (SUCCEEDED(hr))
                {
                    // Get the path to the link target.
                    hr = psl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATAW*)&wfd, 0); //SLGP_SHORTPATH
                    if (SUCCEEDED(hr))
                    {
                        // Get the description of the target.
                        hr = psl->GetDescription(szDescription, MAX_PATH);
                        if (SUCCEEDED(hr))
                        {
                            ok = wcscpy(lpszPath, szGotPath) != NULL;
                        }
                    }
                }
            }
            // Release the pointer to the IPersistFile interface.
            ppf->Release();
        }
        // Release the pointer to the IShellLink interface.
        psl->Release();
    }
    return ok;
}

//extern "C"
//BOOL resolveLnkA(LPCSTR lpszLinkFile, char * lpszPath)
//{
//    HRESULT hr;
//    BOOL ok = FALSE;
//    IShellLinkA * psl;
//
//    char szGotPath[MAX_PATH];
//    char szDescription[MAX_PATH];
//
//    WIN32_FIND_DATAA wfd;
//    *lpszPath = 0;
//
//    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize has already been called.
//    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (LPVOID*)&psl);
//    if (SUCCEEDED(hr))
//    {
//        IPersistFile* ppf;
//        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
//        if (SUCCEEDED(hr))
//        {
//            WCHAR wsz[MAX_PATH];
//
//            // Ensure that the string is Unicode.
//            MultiByteToWideChar(CP_ACP, 0, lpszLinkFile, -1, wsz, MAX_PATH);
//
//            // Load the shortcut.
//            hr = ppf->Load(wsz, STGM_READ);
//            if (SUCCEEDED(hr))
//            {
//                // Resolve the link.
//                hr = psl->Resolve(NULL, 0); // HWND
//                if (SUCCEEDED(hr))
//                {
//                    // Get the path to the link target.
//                    hr = psl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATAA*)&wfd, 0); //SLGP_SHORTPATH
//                    if (SUCCEEDED(hr))
//                    {
//                        // Get the description of the target.
//                        hr = psl->GetDescription(szDescription, MAX_PATH);
//                        if (SUCCEEDED(hr))
//                        {
//                            ok = strcpy(lpszPath, szGotPath) != NULL;
//                        }
//                    }
//                }
//            }
//            // Release the pointer to the IPersistFile interface.
//            ppf->Release();
//        }
//        // Release the pointer to the IShellLink interface.
//        psl->Release();
//    }
//    return ok;
//}
