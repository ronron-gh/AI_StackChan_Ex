# FW Design Information
Notes on FW design, etc.

## Task

| Task name | function | Stack size [bytes] | Priority |
| --- | --- | --- | --- |
| loopTask | Arduino loop task | 8192 | 1 |
| drawLoop | Avatar control | 4 * 1024 | 1 |
| saccade | Avatar control | 1024 | 2 |
| updateBreath | Avatar control | 1024 | 2 |
| blink | Avatar control | 1024 | 2 |
| lipSync | Lip Sync for avatar| 2048 | 1 |
| servo | Servo control synchronized with the avatar | 2048 | 1 |
| battery_check | Battery level check | 2048 | 1 |
| asyncTtsStreamTask | TTS streaming play | 5 * 1024 | 2 |

