# Pippitank

- コアコンポーネント以外は3Dプリンタを活用
- Raspberry Piでカメラ連携、リモート操作したい
- 戦車砲を搭載したい！

## ハードウェア

### パーツ一覧

現状、所持/調達中のパーツ一覧

#### コアパーツ

|カテゴリ      |名称                         |型番                    |数量    |調達状況|メモ                                     |
|--------------|-----------------------------|------------------------|--------|--------|-----------------------------------------|
|モーター      |2430 7200KV                  |不明                    |2       |:+1:    |速すぎる、トルク不足の懸念有             |
|ESC           |ブラシレスモーターの付属品   |不明                    |2       |:+1:    |                                         |
|バッテリー    |Zeee 7.4V 4000mAh 50C 2S LiPo|不明                    |1       |:+1:    |やや古いので、テスト利用に留める         |
|MCU           |ATMEGA328P-PU                |ATMEGA328P-PU           |いっぱい|:+1:    |テスト中はArduino Unoとレベルシフタを使用|
|コンピューター|Raspberry Pi Zero W          |Raspberry Pi Zero W     |1       |:+1:    |テスト中はUSB電源、最終的にはGPIO給電予定|
|コンピューター|カメラモジュール             |Raspberry Pi Camera v2.1|1       |:+1:    |                                         |

ESCのキャリブレーション:

- フルスロットルで電源ONで設定モードに入る
- 後退(1000us)のPWM送信、アイドル(1500us)のPWM送信で再度ビープ音が鳴った後、電源がOFFになる

#### 駆動系パーツ

|カテゴリ|名称                                |型番|数量    |調達状況|メモ                                  |
|--------|------------------------------------|----|--------|--------|--------------------------------------|
|シャフト|3mm x 45mm                          |不明|いっぱい|:+1:    |                                      |
|シャフト|6mm x 45mm                          |不明|いっぱい|:+1:    |                                      |
|ギア    |0.5M 20T 3mm bore bevel gear (brass)|不明|2       |:+1:    |モーターが速すぎるのでおそらく使わない|
|ギア    |0.5M 40T 6mm bore bevel gear (brass)|不明|2       |:+1:    |モーターが速すぎるのでおそらく使わない|
|ギア    |0.4M 18T 3mm bore (copper)          |不明|2       |:+1:    |モーターから3mmシャフトへの変換用     |
|ギア    |0.4M 18T 2mm bore (copper)          |不明|2       |:+1:    |モーターに装着                        |
|ギア    |3mm bore worm gear (copper)         |不明|2       |注文済  |本命                                  |
|ギア    |60T 6mm bore gear (copper)          |不明|2       |注文済  |本命、ウォームギアとのペア            |

#### 電子系パーツ

LiPo電源の分岐、5V供給基板を試作中。\
基板設計: `kicad/projects/power-module`

|カテゴリ  |名称                                        |型番              |数量    |調達状況|メモ                       |
|----------|--------------------------------------------|------------------|--------|--------|---------------------------|
|コネクタ  |XT60PW-M                                    |XT60PW-M          |10      |:+1:    |PCB取付用                  |
|コネクタ  |JST-XH 2PIN                                 |不明              |いっぱい|:+1:    |PCB取付用                  |
|ヒューズ  |低背ヒューズ 58V30A                         |0891030.NXS       |6       |:+1:    |ESC電源前段に取付、短絡対策|
|ヒューズ  |低背ヒューズ 58V5A                          |0891005.NXS       |3       |:+1:    |5V出力前段に取付、短絡対策 |
|ヒューズ  |低背ヒューズホルダー                        |3557-15           |6       |:+1:    |                           |
|DCDC      |表面実装DCDCコンバーター                    |OKL-T/6-W12N-C    |2       |:+1:    |                           |
|抵抗      |チップ抵抗 1/10W100kΩ                       |RC0603J100K       |5000    |:+1:    |                           |
|抵抗      |チップ抵抗 1/10W1.2kΩ                       |RC0603J1K2        |5000    |:+1:    |                           |
|抵抗      |チップ抵抗 1/10W1kΩ                         |RC0603J1K         |5000    |:+1:    |                           |
|抵抗      |チップ抵抗 1/10W330Ω                        |RC0603J330R       |5000    |:+1:    |                           |
|抵抗      |チップ抵抗 1/10W10Ω                         |RC0603J10R        |5000    |:+1:    |                           |
|キャパシタ|チップ積層セラミックコンデンサー 22μF25V X7R|GRM32ER71E226KE15 |10      |:+1:    |                           |
|キャパシタ|チップ積層セラミックコンデンサー 10μF10V B  |GRM21BB31A106KE18L|10      |:+1:    |                           |
|スイッチ  |表面実装用スライドスイッチ                  |SSAJ120100        |2       |:+1:    |5V出力ON/OFF用             |
|LED       |青色チップLED                               |OSBL1608C1A       |20      |:+1:    |5V出力状態表示用           |

#### その他

テストプラットフォームとして、3mm厚の5mm間隔でM3ネジ穴が空けられたユニバーサルプレートを使用。

## ソフトウェア

Gentoo/Neovim環境での開発とする。

```
RPi [UART]-> ATmega328 [PWM]-> ESC
```

RPi側はATmega328との通信を担うサーバーを実装。 \
サーバーに対して任意のクライアントを実装できるようにする。

### Arduino開発環境

下記、それぞれインストール

[https://github.com/arduino/arduino-cli](https://github.com/arduino/arduino-cli) \
[https://github.com/arduino/arduino-language-server](https://github.com/arduino/arduino-language-server) \
[https://github.com/neovim/nvim-lspconfig/blob/master/doc/configs.md#arduino_language_server](https://github.com/neovim/nvim-lspconfig/blob/master/doc/configs.md#arduino_language_server) 

#### arduino-cli

設定ファイルの生成

```bash
arduino-cli config init
```

ボードまわりの情報を更新

```bash
arduino-cli core update-index
```

Arduino Uno用にボードのコアパッケージをインストール

```bash
arduino-cli core install arduino:avr
```

#### arduino-language-server

スケッチの新規作成

```bash
arduino-cli sketch new example
```

sketch.yamlの作成

```bash
arduino-cli board attach -p /dev/ttyACM0 -b arduino:avr:uno example/example.ino
```

Neovim側でarduino-language-serverを有効にする

```lua
vim.lsp.enable('arduino_language_server')
```

### スケッチのコンパイルとアップロード

コンパイル

```bash
arduino-cli compile example
```

スケッチのアップロード

```bash
arduino-cli upload example
```

### クロスコンパイル環境の構築

[https://wiki.gentoo.org/wiki/Crossdev](https://wiki.gentoo.org/wiki/Crossdev)

crossdevとeselect-repositoryをインストール

```bash
emerge -a crossdev eselect-repository
```

crossdevのレポジトリ作成

```bash
eselect repository create crossdev
```

ラズパイ用のtoolchainをビルド

```bash
crossdev --target arm-linux-gnueabihf
```

### Neovim環境の構築

言語サーバーはclangdを使う

[https://github.com/neovim/nvim-lspconfig/blob/master/doc/configs.md#clangd](https://github.com/neovim/nvim-lspconfig/blob/master/doc/configs.md#clangd)

```lua
vim.lsp.enable('clangd')
```

レポジトリ直下に`.clangd`を作成

```
CompileFlags:
  Add:
    - --target=arm-linux-gnueabihf
    - -I/usr/arm-linux-gnueabihf/usr/include
```

### コンパイル

```bash
make
```
