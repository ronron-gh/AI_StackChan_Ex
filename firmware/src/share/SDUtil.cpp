
#include <Arduino.h>
#include <M5Unified.h>
#include <stdio.h>
#include <SD.h>
#include <SPIFFS.h>

int read_sd_file(const char* filename, char* buf, int buf_size)
{
  int ret;
  auto fs = SD.open(filename, FILE_READ);
  
  if(fs) {
    size_t sz = fs.size();
    if(sz >= buf_size){
      Serial.printf("SD file size (%d) exceeds buf size (%d).\n", sz, buf_size);
      ret = 0;
    }
    else{
      fs.read((uint8_t*)buf, sz);
      buf[sz] = 0;
      fs.close();
      ret = sz;
    }
  }
  else{
    Serial.println("Failed to open SD.");
    ret = 0;
  }
  return ret;
}


int read_line_from_buf(char* buf, char* out)
{
  static int n_total = 0;
  static char* buf_ = nullptr;
  int n, ret;

  if(buf != nullptr){
    buf_ = buf;
  }

  if(buf_ == nullptr){
    Serial.println("read_line_from_buf: buffer is nullptr.");
    return 0;
  }

  ret = sscanf(buf_, "%s%n", out, &n);
  buf_ += n;

  return ret;
}


// SDカードのファイルをSPIFFSにコピー
bool copySDFileToSPIFFS(const char *path, bool forced) {
  
  Serial.println("Copy SD File to SPIFFS.");

  if(!SPIFFS.begin(true)){
    Serial.println("Failed to mount SPIFFS.");
    return false;
  }

  if (SPIFFS.exists(path) && !forced) {
	  Serial.println("File already exists in SPIFFS.");
	  return true;
  }

  if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    Serial.println("Failed to mount SD.");
    return false;
  }

  File fsrc = SD.open(path, FILE_READ);
  File fdst = SPIFFS.open(path, FILE_WRITE);
  if(!fsrc || !fdst) {
    Serial.println("Failed to open file.");
    return false;
  }

  uint8_t *buf = new uint8_t[4096];
  if (!buf) {
	  Serial.println("Failed to allocate buffer.");
	  return false;
  }

  size_t len, size, ret;
  size = len = fsrc.size();
  while (len) {
	  size_t s = len;
	  if (s > 4096){
		  s = 4096;
    }  

	  fsrc.read(buf, s);
	  if ((ret = fdst.write(buf, s)) < s) {
		  Serial.printf("write failed: %d - %d\n", ret, s);
		  return false;
	  }
	  len -= s;
	  Serial.printf("%d / %d\n", size - len, size);
  }
 
  delete[] buf;
  fsrc.close();
  fdst.close();

  if (!SPIFFS.exists(path)) {
	  Serial.println("no file in SPIFFS.");
	  return false;
  }
  fdst = SPIFFS.open(path);
  len = fdst.size();
  fdst.close();
  if (len != size) {
	 Serial.println("size not match.");
	 return false;
  }
  Serial.println("*** Done. ***\r\n");
  return true;
}

