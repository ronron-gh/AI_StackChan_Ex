#include <M5Unified.h>
#include "DebugTools.h"


void check_heap_free_size(void){
  Serial.printf("===============================================================\n");
  Serial.printf("Check free heap size\n");
  Serial.printf("===============================================================\n");
  //Serial.printf("esp_get_free_heap_size()                              : %6d\n", esp_get_free_heap_size() );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DMA)               : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM)            : %6d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_INTERNAL)          : %6d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DEFAULT)           : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) );

}

void check_heap_largest_free_block(void){
  Serial.printf("===============================================================\n");
  Serial.printf("Check largest free heap block\n");
  Serial.printf("===============================================================\n");
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DMA)      : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)   : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)  : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) );

}

#if 0 // vTaskListが使えなかった
void check_task_status(void){
  char msg_buffer[512];
  vTaskList(msg_buffer);
  Serial.printf("%s", msg_buffer);
}
#endif

uint32_t get_elapsed_time_micro(const char* str){
  static uint32_t prev = 0;
  uint32_t now = 0;
  uint32_t elapsedTime = 0;
  now = micros();
  if(prev == 0){
    prev = now;
  }
  elapsedTime = now - prev;
  prev = now;
  Serial.printf("%s: %d [us]\n", str, elapsedTime);
  return elapsedTime;
}


void i2c_scan(TwoWire& wire)
{
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++) {
    wire.beginTransmission(address);
    error = wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("done\n");
  }

}

void check_board(void)
{
  switch(M5.getBoard()) {
    case m5gfx::board_t::board_M5StackCore2:
      Serial.println("Board:M5StackCore2");
      break;
    case m5gfx::board_t::board_M5StackCoreS3:
      Serial.println("Board:M5StackCoreS3");
      break;
    case m5gfx::board_t::board_M5StackCoreS3SE:
      Serial.println("Board:M5StackCoreS3SE");
      break;
    case m5gfx::board_t::board_M5AtomS3R:
      Serial.println("Board:M5AtomS3R");      
      break;
    case m5gfx::board_t::board_M5StackChan:
      Serial.println("Board:M5StackChan");
      break;
    default:
      Serial.println("Board:Unknown");
      break;
  }
}