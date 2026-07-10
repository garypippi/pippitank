# Pippitank

ラズパイ戦車制作プロジェクト

- 電源: 2S LiFe (タミヤ LF1100-6.6V)
- Raspberry Piでカメラ連携、リモート操作
- プッシュ型ソレノイドでBB弾発射

```
+----------+
| 2S LiFe  |
+----+-----+
     |
+----+-----------------+  UART  +-----------------------+
| pcb#pippitank-engine +--------+ pcb#pippitank-chassis |
+----+-----------------+        +----+--+-------+-------+
     |       |                       |  |       |
+-------+  +-------+      Dshot#2    |  |       | UART (Slip Ring)
| ESC#1 |  | ESC#2 |-----------------+  |       |
+----+--+  +-------+                    |    +----------------------+
     |                    Dshot#1       |    | pcb#pippitank-turret |
     +----------------------------------+    +----------------------+
                                                |
                                             ?? Planning... ??
```

## コマンド

### ビルド

```sh
cmake -S . -B build
cmake --build build
```

### RPi側

カメラストリーム配信。`--mode 1640:1232` はフル画角化のため（未指定だと中央クロップで約2.5倍ズームになる）:

```sh
rpicam-vid -t 0 --inline \
  --mode 1640:1232 \
  --width 640 --height 480 \
  --framerate 30 --listen -o tcp://0.0.0.0:8888
```

デーモン起動:

```sh
./build/src/pippitankd/pippitankd --listen 0.0.0.0:5000 --serial /dev/ttyS0
```

### ホスト側

操縦に使う入力デバイスの確認:

```sh
ls /dev/input/by-id
```

UI起動。`--log` は省略可（テレメトリCSVを記録する場合のみ）:

```sh
./build/src/pippitank-ui/pippitank-ui \
  --server pippitank:5000 \
  --video tcp://pippitank:8888 \
  --input /dev/input/event8 \
  --log logs/telemetry.csv
```
