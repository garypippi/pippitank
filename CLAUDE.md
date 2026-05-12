# Pippitank

ラズパイ戦車制作プロジェクト

- 電源: 2S Li-Po
- Raspberry Piでカメラ連携、リモート操作
- プッシュ型ソレノイドでBB弾発射

```
+----------+
| 2S Li-Po |
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

## 設計方針

2S Li-Poが発火すると重大事故となるので、短絡等の脆弱性に最大限注意して設計する。

## 進行中のPCB設計

- ./pcb/pippitank-engine 電源分岐基板
    - REV_A実装済
    - ファーム作成済
- ./pcb/pippitank-esc-adaptor ESCアダプタ基板
    - 構想段階

## TODO

- シャーシ設計(3DCAD)
- ギアボックス設計(3DCAD)
- ファームリファクタ
- ./pcb/pippitank-esc-adaptor/rev_a 設計
- ./pcb/pippitank-engine/rev_b 設計
