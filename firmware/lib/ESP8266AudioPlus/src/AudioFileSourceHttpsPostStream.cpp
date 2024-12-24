/*
  AudioFileSourceHttpsPostStream
  Streaming HTTP source

  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(ESP32) || defined(ESP8266)

#include "AudioFileSourceHttpsPostStream.h"
//#include "rootCACertificate.h"

AudioFileSourceHttpsPostStream::AudioFileSourceHttpsPostStream()
{
  pos = 0;
  reconnectTries = 0;
  saveURL[0] = 0;
}

AudioFileSourceHttpsPostStream::AudioFileSourceHttpsPostStream(const char *url, const char* root_ca)
{
  saveURL[0] = 0;
  reconnectTries = 0;
  rootCACertificate = root_ca;
  //open(url);
}

void AudioFileSourceHttpsPostStream::addHeader(const String& name, const String& value)
{
  http.addHeader(name, value);
}

void AudioFileSourceHttpsPostStream::addData(String json)
{
  jsonData = json;
}

bool AudioFileSourceHttpsPostStream::open(const char *url)
{
  pos = 0;
  client.setCACert(rootCACertificate);
  http.begin(client, url);
  http.setReuse(true);
#ifndef ESP32
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
#endif
  //int code = http.GET();
  int code = http.POST(jsonData);
  if (code != HTTP_CODE_OK) {
    http.end();
    cb.st(STATUS_HTTPFAIL, PSTR("Can't open HTTP request"));
    Serial.printf("Can't open HTTP request. code = %d\n", code);
    return false;
  }
  size = http.getSize();
  strncpy(saveURL, url, sizeof(saveURL));
  saveURL[sizeof(saveURL)-1] = 0;
  return true;
}

AudioFileSourceHttpsPostStream::~AudioFileSourceHttpsPostStream()
{
  http.end();
}

uint32_t AudioFileSourceHttpsPostStream::read(void *data, uint32_t len)
{
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHttpsPostStream::read passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, false);
}

uint32_t AudioFileSourceHttpsPostStream::readNonBlock(void *data, uint32_t len)
{
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHttpsPostStream::readNonBlock passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, true);
//  return readInternal(data, len, false);
}

uint32_t AudioFileSourceHttpsPostStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected()) {
    cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      char buff[64];
      sprintf_P(buff, PSTR("Attempting to reconnect, try %d"), i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        cb.st(STATUS_RECONNECTED, PSTR("Stream reconnected"));
        break;
      }
    }
    if (!http.connected()) {
      cb.st(STATUS_DISCONNECTED, PSTR("Unable to reconnect"));
      return 0;
    }
  }
  if ((size > 0) && (pos >= size)) return 0;

  WiFiClient *stream = http.getStreamPtr();

  // Can't read past EOF...
  if ( (size > 0) && (len > (uint32_t)(pos - size)) ) len = pos - size;

  if (!nonBlock) {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500)) yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail) {
    cb.st(STATUS_NODATA, PSTR("No stream data available"));
    http.end();
    //goto retry;   //ElevenLabsだと無限に繰り返してしまうためreturnに変更
    return 0;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;

  int read = stream->read(reinterpret_cast<uint8_t*>(data), len);
  pos += read;
  return read;
}

bool AudioFileSourceHttpsPostStream::seek(int32_t pos, int dir)
{
  audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHttpsPostStream::seek not implemented!"));
  (void) pos;
  (void) dir;
  return false;
}

bool AudioFileSourceHttpsPostStream::close()
{
  http.end();
  return true;
}

bool AudioFileSourceHttpsPostStream::isOpen()
{
  return http.connected();
}

uint32_t AudioFileSourceHttpsPostStream::getSize()
{
  return size;
}

uint32_t AudioFileSourceHttpsPostStream::getPos()
{
  return pos;
}

#endif