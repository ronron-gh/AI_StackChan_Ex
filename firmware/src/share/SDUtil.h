#ifndef _SD_UTIL_H
#define _SD_UTIL_H

int read_sd_file(const char* filename, char* buf, int buf_size);
int read_line_from_buf(char* buf, char* out);
bool copySDFileToSPIFFS(const char *path, bool forced);
size_t copySDFileToRAM(const char *path, uint8_t *out, int bufSize);

#endif