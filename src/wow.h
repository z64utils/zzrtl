/* wow.h - v0.01
   helpful functions

	Before #including,
	
		#define WOW_IMPLEMENTATION
	
	in the file that you want to have the implementation.
	
	If you want the implementation to be available only in
	one source file, do this before #including:
	
		#define WOW_API_PREFIX static
*/

#ifndef INCLUDE_WOW_H
#define INCLUDE_WOW_H

#include <stddef.h> /* size_t */
#include <stdio.h> /* file ops */
#include <stdlib.h> /* alloc */
#include <sys/stat.h> /* stat */
#include <string.h> /* strdup */
#include <unistd.h> /* chdir, getcwd */
#include <stdarg.h>

#ifndef WOW_API_PREFIX
#	define WOW_API_PREFIX
#endif

#if defined(_WIN32)
#	include <windows.h>
#undef near
#undef far
#endif

WOW_API_PREFIX
void *
wow_utf8_to_wchar(const char *str);

/* safe malloc */
WOW_API_PREFIX
void *
wow_malloc_safe(size_t size);

WOW_API_PREFIX
char *
wow_wchar_to_utf8(void *wstr);


WOW_API_PREFIX
void
wow_die(const char *fmt, ...);


/* converts argv[] from wchar to char win32, in place */
WOW_API_PREFIX
void *
wow_conv_args(int argc, void *argv[]);


/* returns non-zero if path is a directory */
WOW_API_PREFIX
int
wow_is_dir_w(void const *path);


/* returns non-zero if path is a directory */
WOW_API_PREFIX
int
wow_is_dir(char const *path);


/* fread abstraction that falls back to buffer-based fread *
 * if a big fread fails; if that still fails, returns 0    */
WOW_API_PREFIX
size_t
wow_fread_bytes(void *ptr, size_t bytes, FILE *stream);


/* fwrite abstraction that falls back to buffer-based fwrite *
 * if a big fwrite fails; if that still fails, returns 0     */
WOW_API_PREFIX
size_t
wow_fwrite_bytes(const void *ptr, size_t bytes, FILE *stream);


/* fread abstraction that falls back to buffer-based fread *
 * if a big fread fails; if that still fails, returns 0    */
WOW_API_PREFIX
size_t
wow_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);


/* fwrite abstraction that falls back to buffer-based fwrite *
 * if a big fwrite fails; if that still fails, returns 0     */
WOW_API_PREFIX
size_t
wow_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);


/* fopen abstraction for utf8 support on windows win32 */
WOW_API_PREFIX
FILE *
wow_fopen(char const *name, char const *mode);


/* mkdir */
WOW_API_PREFIX
int
wow_mkdir(char const *path);


/* chdir */
WOW_API_PREFIX
int
wow_chdir(char const *path);


/* getcwd */
WOW_API_PREFIX
char *
wow_getcwd(char *buf, size_t size);


/* getcwd_safe */
WOW_API_PREFIX
char *
wow_getcwd_safe(char *buf, size_t size);


/* system */
WOW_API_PREFIX
int
wow_system(char const *path);


/* system_gui */
WOW_API_PREFIX
int
wow_system_gui(char const *name, const char *param);

#ifdef WOW_IMPLEMENTATION


WOW_API_PREFIX
void
wow_die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

WOW_API_PREFIX
void *
wow_malloc_safe(size_t size)
{
	void *result;
	
	result = malloc(size);
	
	if (!result)
		wow_die("allocation failure");
	
	return result;
}

WOW_API_PREFIX
void *
wow_utf8_to_wchar(const char *str)
{
#ifdef UNICODE
extern __declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int cp, unsigned long flags, const char *str, int cbmb, wchar_t *widestr, int cchwide);
    wchar_t *wstr;
    int wstr_sz = (strlen(str) + 1) * 16;//sizeof(*wstr);
    wstr = wow_malloc_safe(wstr_sz);
    MultiByteToWideChar(65001/*utf8*/, 0, str, -1, wstr, wstr_sz);
    return wstr;
#else
    return strdup(str);
#endif
}

WOW_API_PREFIX
char *
wow_wchar_to_utf8_buf(void *wstr, void *dst, int dst_max)
{
#ifdef UNICODE
extern __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t *widestr, int cchwide, char *str, int cbmb, const char *defchar, int *used_default);
    WideCharToMultiByte(65001/*utf8*/, 0, wstr, -1, dst, dst_max, NULL, NULL);
    return dst;
#else
    (void)dst_max; /* unused parameter */
    return strcpy(dst, wstr);
#endif
}

WOW_API_PREFIX
char *
wow_wchar_to_utf8(void *wstr)
{
#ifdef UNICODE
extern __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t *widestr, int cchwide, char *str, int cbmb, const char *defchar, int *used_default);
    char *str;
    int str_sz = (wcslen(wstr) + 1) * sizeof(*str);
    str = wow_malloc_safe(str_sz);
    WideCharToMultiByte(65001/*utf8*/, 0, wstr, -1, str, str_sz, NULL, NULL);
    return str;
#else
    return strdup(wstr);
#endif
}

WOW_API_PREFIX
char *
wow_wchar_to_utf8_inplace(void *wstr)
{
#ifdef UNICODE
extern __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t *widestr, int cchwide, char *str, int cbmb, const char *defchar, int *used_default);
    char buf[4096];
    char *str;
    int str_sz = (wcslen(wstr) + 1) * sizeof(*str);
    if (str_sz >= sizeof(buf))
        str = wow_malloc_safe(str_sz);
    else
        str = buf;
    WideCharToMultiByte(65001/*utf8*/, 0, wstr, -1, str, str_sz, NULL, NULL);
    memcpy(wstr, str, str_sz);
    if (str != buf)
        free(str);
    return wstr;
#else
    return wstr;
#endif
}


/* argument abstraction: converts argv[] from wchar to char win32 */
WOW_API_PREFIX
void *
wow_conv_args(int argc, void *argv[])
{
#ifdef UNICODE
	int i;
	for (i=0; i < argc; ++i)
	{
//		fprintf(stderr, "[%d]: %s\n", i, argv[i]);
//		fwprintf(stderr, L"[%d]: %s\n", i, (wchar_t*)argv[i]);
		argv[i] = wow_wchar_to_utf8_inplace(argv[i]);
	}
#else
	(void)argc; /* unused parameter */
#endif
	return argv;
}

/* returns non-zero if path is a directory */
WOW_API_PREFIX
int
wow_is_dir_w(void const *path)
{
	struct stat s;
#if (_WIN32 && UNICODE)
	if (wstat(path, &s) == 0)
#else
	if (stat(path, &s) == 0)
#endif
	{
		if (s.st_mode & S_IFDIR)
			return 1;
	}
	
	return 0;
}


/* returns non-zero if path is a directory */
WOW_API_PREFIX
int
wow_is_dir(char const *path)
{
	int rv;
	void *wpath = 0;
	
#if (_WIN32 && UNICODE)
	wpath = wow_utf8_to_wchar(path);
	rv = wow_is_dir_w(wpath);
#else
	rv = wow_is_dir_w(path);
#endif
	if (wpath)
		free(wpath);
	
	return rv;
}


/* fread abstraction that falls back to buffer-based fread *
 * if a big fread fails; if that still fails, returns 0    */
WOW_API_PREFIX
size_t
wow_fread_bytes(void *ptr, size_t bytes, FILE *stream)
{
	if (!stream || !ptr || !bytes)
		return 0;
	
	unsigned char *ptr8 = ptr;
	size_t Oofs = ftell(stream);
	size_t bufsz = 1024 * 1024; /* 1 mb at a time */
	size_t Obytes = bytes;
	size_t rem;
	
	fseek(stream, 0, SEEK_END);
	rem = ftell(stream) - Oofs;
	fseek(stream, Oofs, SEEK_SET);
	
	if (bytes > rem)
		bytes = rem;
	
	/* everything worked */
	if (fread(ptr, 1, bytes, stream) == bytes)
		return Obytes;
	
	/* failed: try falling back to slower buffered read */
	fseek(stream, Oofs, SEEK_SET);
	while (bytes)
	{
		/* don't read past end */
		if (bytes < bufsz)
			bufsz = bytes;
		if (bufsz > rem)
		{
			bytes = rem;
			bufsz = rem;
		}
		
		/* still failed */
		if (fread(ptr8, 1, bufsz, stream) != bufsz)
			return 0;
		
		/* advance */
		ptr8 += bufsz;
		bytes -= bufsz;
		rem -= bufsz;
	}
	
	/* success */
	return Obytes;
}


/* fwrite abstraction that falls back to buffer-based fwrite *
 * if a big fwrite fails; if that still fails, returns 0     */
WOW_API_PREFIX
size_t
wow_fwrite_bytes(const void *ptr, size_t bytes, FILE *stream)
{
	if (!stream || !ptr || !bytes)
		return 0;
	
	const unsigned char *ptr8 = ptr;
	size_t bufsz = 1024 * 1024; /* 1 mb at a time */
	size_t Obytes = bytes;
	
	/* everything worked */
	if (fwrite(ptr, 1, bytes, stream) == bytes)
		return bytes;
	
	/* failed: try falling back to slower buffered read */
	while (bytes)
	{
		/* don't read past end */
		if (bytes < bufsz)
			bufsz = bytes;
		
		/* still failed */
		if (fwrite(ptr8, 1, bufsz, stream) != bufsz)
			return 0;
		
		/* advance */
		ptr8 += bufsz;
		bytes -= bufsz;
	}
	
	/* success */
	return Obytes;
}


/* fread abstraction that falls back to buffer-based fread *
 * if a big fread fails; if that still fails, returns 0    */
WOW_API_PREFIX
size_t
wow_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (!stream || !ptr || !size || !nmemb)
		return 0;
	
	if (wow_fread_bytes(ptr, size * nmemb, stream) == size * nmemb)
		return nmemb;
	
	return 0;
}


/* fwrite abstraction that falls back to buffer-based fwrite *
 * if a big fwrite fails; if that still fails, returns 0     */
WOW_API_PREFIX
size_t
wow_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (!stream || !ptr || !size || !nmemb)
		return 0;
	
	if (wow_fwrite_bytes(ptr, size * nmemb, stream) == size * nmemb)
		return nmemb;
	
	return 0;
}


/* fopen abstraction for utf8 support on windows win32 */
WOW_API_PREFIX
FILE *
wow_fopen(char const *name, char const *mode)
{
#ifdef UNICODE
	void *wname = 0;
	void *wmode = 0;
	FILE *fp = 0;
	
	wname = wow_utf8_to_wchar(name);
	if (!wname)
		goto L_cleanup;
	
	/* TODO eventually, an error message would be cool */
	if (wow_is_dir_w(wname))
		goto L_cleanup;
	
	wmode = wow_utf8_to_wchar(mode);
	if (!wmode)
		goto L_cleanup;
	
	fp = _wfopen(wname, wmode);
	
L_cleanup:
	if (wname) free(wname);
	if (wmode) free(wmode);
	if (fp)
		return fp;
	return 0;
#else
	/* TODO eventually, an error message would be cool */
	if (wow_is_dir_w(name))
		return 0;
	return fopen(name, mode);
#endif
}


/* mkdir */
WOW_API_PREFIX
int
wow_mkdir(char const *path)
{
#if defined(_WIN32) && defined(UNICODE)
extern int _wmkdir(const wchar_t *);
	void *wname = 0;
	int rval;
	
	wname = wow_utf8_to_wchar(path);
	if (!wname)
		return -1;
	
	rval = _wmkdir(wname);
	
	if (wname)
		free(wname);
	
	return rval;
#elif defined(_WIN32) /* win32 no unicode */
extern int _mkdir(const char *);
	return _mkdir(path);
#else /* ! _WIN32 */
	return mkdir(path, 0777);
#endif
}


/* chdir */
WOW_API_PREFIX
int
wow_chdir(char const *path)
{
#if defined(_WIN32) && defined(UNICODE)
extern int _wchdir(const wchar_t *);
	void *wname = 0;
	int rval;
	
	wname = wow_utf8_to_wchar(path);
	if (!wname)
		return -1;
	
	rval = _wchdir(wname);
	
	if (wname)
		free(wname);
	
	return rval;
#elif defined(_WIN32) /* win32 no unicode */
extern int _chdir(const char *);
	return _chdir(path);
#else /* ! _WIN32 */
	return chdir(path);
#endif
}


/* getcwd */
WOW_API_PREFIX
char *
wow_getcwd(char *buf, size_t size)
{
#if defined(_WIN32) && defined(UNICODE)
//extern int _wgetcwd(const wchar_t *, int);
extern _CRTIMP wchar_t *__cdecl _wgetcwd(wchar_t *_DstBuf,int _SizeInWords);
	wchar_t wname[4096];
	
	if (!buf || !size)
		return 0;
	
	if (!_wgetcwd(wname, sizeof(wname) / sizeof(wname[0])))
		return 0;
	
	return wow_wchar_to_utf8_buf(wname, buf, size);
#elif defined(_WIN32) /* win32 no unicode */
//extern char *_getcwd(char *, int);
	return _getcwd(buf, size);
#else /* ! _WIN32 */
	return getcwd(buf, size);
#endif
}


/* getcwd_safe */
WOW_API_PREFIX
char *
wow_getcwd_safe(char *buf, size_t size)
{
	char *result = wow_getcwd(buf, size);
	
	if (!result)
		wow_die("failed to get current working directory");
	
	return result;
}


/* system */
WOW_API_PREFIX
int
wow_system(char const *path)
{
#if defined(_WIN32) && defined(UNICODE)
	void *wname = 0;
	int rval;
	
	wname = wow_utf8_to_wchar(path);
	if (!wname)
		return -1;
	
	rval = _wsystem(wname);
	
	if (wname)
		free(wname);
	
	return rval;
#else /* not win32 unicode */
	#ifdef _WIN32
	return system(path);
	#else
	// allow win32 paths on linux builds
	if (strchr(path, '\\'))
	{
		char *tmp = strdup(path);
		int rval;
		
		for (char *c = tmp; *c; ++c)
			if (*c == '\\')
				*c = '/';
		
		rval = system(tmp);
		free(tmp);
		return rval;
	}
	else
	{
		return system(path);
	}
	#endif
#endif
}


/* system_gui */
WOW_API_PREFIX
int
wow_system_gui(char const *name, const char *param)
{
#if defined(_WIN32)
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	int rval = 0 /*success */;
//extern int ShellExecuteA(void *hwnd, void *op, void *file, void *param, void *dir, int cmd);
//extern int ShellExecuteW(void *hwnd, void *op, void *file, void *param, void *dir, int cmd);
//const int SW_SHOWNORMAL = 1;
	#if defined(UNICODE)
		void *wname = 0;
		void *wparam = 0;
		
		wname = wow_utf8_to_wchar(name);
		if (!wname)
		{
			return -1;
		}
		wparam = wow_utf8_to_wchar(param);
		if (!wparam)
		{
			free(wname);
			return -1;
		}
		
#if 0
		if (CreateProcessW(
			wname, wparam
			, NULL, NULL
			, FALSE
			, CREATE_NO_WINDOW
			, NULL
			, NULL
			, &si, &pi)
		)
		{
		//WaitForSingleObject(pi.hProcess, INFINITE);
		//CloseHandle(pi.hProcess);
		//CloseHandle(pi.hThread);
		}
		else
			rval = 1;
#else
		rval = (int)ShellExecuteW(NULL, L"open", wname, wparam, L".", SW_SHOWNORMAL);
		rval = rval <= 32;
#endif
		
		free(wname);
		free(wparam);
	#else /* win32 non-unicode */
#if 0
		if (CreateProcessA(
			name, x
			, NULL, NULL
			, FALSE
			, CREATE_NO_WINDOW
			, NULL
			, NULL
			, &si, &pi)
		)
		{
		//WaitForSingleObject(pi.hProcess, INFINITE);
		//CloseHandle(pi.hProcess);
		//CloseHandle(pi.hThread);
		}
		else
			rval = 1;
#else
		rval = (int)ShellExecuteA(NULL, "open", name, param, ".", SW_SHOWNORMAL);
		rval = rval <= 32;
#endif
	#endif
	return rval;//rval <= 32;
#else /* not win32 unicode */
	char *x = wow_malloc_safe(strlen(name) + strlen(param) + 128);
	if (!x)
		return -1;
	strcpy(x, "\"");
	strcat(x, name);
	strcat(x, "\" ");
	strcat(x, param);
	int rval = system(x);
	free(x);
	return rval;
#endif
}

#endif /* WOW_IMPLEMENTATION */

#endif /* INCLUDE_WOW_H */

