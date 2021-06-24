/* expands to use wide versions of dirent functions, etc */
/* must be #included after dirent.h and wow.h */

#if defined(_WIN32) && defined(UNICODE)
#	define  wow_DIR          _WDIR
#	define  wow_dirent       _wdirent
static
wow_DIR *
wow_opendir(const char *path)
{
	void *wpath = wow_utf8_to_wchar(path);
	if (!wpath)
		return NULL;
	
	wow_DIR *rv = _wopendir(wpath);
	
	free(wpath);
	
	return rv;
}
static
struct wow_dirent *
wow_readdir(wow_DIR *dir)
{
	struct wow_dirent *ep = _wreaddir(dir);
	if (!ep)
		return 0;
	
	/* convert d_name to utf8 for working on them directly */
	char *str = wow_wchar_to_utf8(ep->d_name);
	memcpy(ep->d_name, str, strlen(str) + 1);
	free(str);
	
	return ep;
}
#	define  wow_closedir     _wclosedir
#	define  wow_dirent_char  wchar_t

#else /* not win32 unicode */
#	define  wow_DIR          DIR
#	define  wow_dirent       dirent
#	define  wow_opendir      opendir
#	define  wow_readdir      readdir
#	define  wow_closedir     closedir
#	define  wow_dirent_char  char
#endif
