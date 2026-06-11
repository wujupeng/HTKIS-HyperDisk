#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "wininet.lib")

int download(const char *url, const char *outfile)
{
    char scheme[16], host[256], path[2048];
    URL_COMPONENTSA uc = {0};
    uc.dwStructSize = sizeof(uc);
    uc.lpszScheme = scheme; uc.dwSchemeLength = 16;
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;

    if (!InternetCrackUrlA(url, 0, 0, &uc)) {
        fprintf(stderr, "CrackUrl failed: %lu\n", GetLastError());
        return 1;
    }

    HINTERNET hinet = InternetOpenA("hd_dl/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hinet) return 1;

    HINTERNET hconn = InternetConnectA(hinet, host, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hconn) { InternetCloseHandle(hinet); return 1; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0;
    HINTERNET hreq = HttpOpenRequestA(hconn, "GET", path, NULL, NULL, NULL, flags | INTERNET_FLAG_RELOAD, 0);
    if (!hreq) { InternetCloseHandle(hconn); InternetCloseHandle(hinet); return 1; }

    if (!HttpSendRequestA(hreq, NULL, 0, NULL, 0)) {
        fprintf(stderr, "SendRequest failed: %lu\n", GetLastError());
        InternetCloseHandle(hreq); InternetCloseHandle(hconn); InternetCloseHandle(hinet);
        return 1;
    }

    DWORD status_code = 0, sz = sizeof(status_code);
    HttpQueryInfoA(hreq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status_code, &sz, 0);
    if (status_code != 200) {
        fprintf(stderr, "HTTP %lu\n", status_code);
        InternetCloseHandle(hreq); InternetCloseHandle(hconn); InternetCloseHandle(hinet);
        return 1;
    }

    FILE *fp = fopen(outfile, "wb");
    if (!fp) {
        fprintf(stderr, "Cannot create %s\n", outfile);
        InternetCloseHandle(hreq); InternetCloseHandle(hconn); InternetCloseHandle(hinet);
        return 1;
    }

    char buf[8192];
    DWORD read = 0;
    long total = 0;
    while (InternetReadFile(hreq, buf, sizeof(buf), &read) && read > 0) {
        fwrite(buf, 1, read, fp);
        total += read;
    }
    fclose(fp);

    InternetCloseHandle(hreq);
    InternetCloseHandle(hconn);
    InternetCloseHandle(hinet);

    fprintf(stderr, "OK %ld bytes -> %s\n", total, outfile);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: hd_dl.exe URL outfile [URL2 outfile2 ...]\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        int r = download(argv[i], argv[i + 1]);
        if (r) rc = r;
    }
    return rc;
}
