#ifndef FIND_BYTES_H_INCLUDED
#define FIND_BYTES_H_INCLUDED


/* find text string `ndl` inside raw data `hay`, with stride */
void *
find_text_stride
(void *_hay, int hay_sz, char *ndl, int stride, int only1);


/* find text string `ndl` inside raw data `hay` */
void *
find_text(void *hay, int hay_sz, char *ndl, int only1);


/* find byte string `ndl` inside raw data `hay`, with stride */
void *
find_bytes_stride
(void *_hay, int hay_sz, char *ndl, int stride, int only1);


/* find byte string `ndl` inside raw data `hay` */
void *
find_bytes(void *hay, int hay_sz, char *ndl, int only1);


#endif /* FIND_BYTES_H_INCLUDED */

