# pippitank-engine

2S Li-Po電源をESC#1, ESC#2, 車体システムへ分岐させる基板。

## 機能

- Pch-MOSFETで主電源ON/OFF
- MCUからNch-MOSFET経由でPch-MOSFETを制御
- 電圧監視
- 温度監視
- 電流監視

## 電圧監視地点

- 2S Li-Po電源 (VCC)
- 2S Li-Poセル#1 (バランスコネクタ経由)
- Pch-MOSFET直後

## 電流監視地点

各電源供給地点

- ESC#1
- ESC#2
- chassis/turret

## 温度測定地点

Pch-MOSFET近傍
