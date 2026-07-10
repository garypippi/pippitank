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

## 命名規約

- ビルドtarget / ディレクトリ名: `pippitank-*` (デーモンのみ `pippitankd`)
- C++ namespace: `ptank` (`pt::` は OpenCV `cv::Point pt;` 等とのshadowリスクで却下)
- wire定数prefix: `PTANK_*` (Arduino側の旧 `PT_*` はC++17移行時に一括リネーム予定、ホストとArduinoで混在させない)

## 設計方針

リチウム系バッテリーの発火は重大事故となるので、短絡等の脆弱性に最大限注意して設計する（Li-Poより安全なLiFeに移行済みだが方針は継続）。

## 進行中のPCB設計

- ./pcb/pippitank-engine 電源分岐基板
    - rev_a 実装済・ファーム作成済
    - rev_b 設計予定 (5VはOKL-T6 buck採用、Q1ソフトスタート構成確定済)
- ./pcb/pippitank-esc-adaptor ESCアダプタ基板
    - rev_a 実装済・castellation実測済・実用充足
    - rev_b は凍結 (rev_a 故障時に着手、申し送りは rev_b/README.md)

## TODO

- シャーシ設計(3DCAD、底面から着手予定)
- BB弾フィーダー機構
- ./pcb/pippitank-engine/rev_b 設計
- UIリファクタ / シャント校正 / LVC(v_cell_1監視)
- src/ 移行残骸の削除 (pippitank-tui, pippitank-cli, pippitankd/main.c)、pippitank-cmd の C++17化判断、Arduino リネーム (PT_* → PTANK_*)
