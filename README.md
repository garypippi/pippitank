# Pippitank

ラジコン戦車制作プロジェクト

- 2S Li-Po
- Raspberry Piでカメラ連携、リモート操作
- プッシュ型ソレノイドでBB弾発射

## ハードウェア

システム構成

```
2S Li-Po --- 電源基板(ATtiny1604)
              |
              | UART
              |
             車体基板(ATmega328PB or Raspberry Pi Pico)
              |        |                      |
              |        +--- ESC --- Moter     +--- ESC --- Moter
              |
              | UART(スリップリング)
              |
             砲塔基板 (Raspberry Pi Zero W/ATmega328PB)
```

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
