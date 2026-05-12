#!/usr/bin/env python3
"""pippitank-ui の CSV テレメトリログをインタラクティブな HTML グラフに変換する。

入力 CSV は pippitank-ui --log が吐く以下の列を前提とする:
    t_unix_ms, l, r, v_cell_1, v_bat, v_q,
    i_esc_1, i_esc_2, i_sys, temp_c, uptime_ms

使い方:
    python3 agg/telemetry.py /tmp/pippitank.csv
    python3 agg/telemetry.py /tmp/pippitank.csv --out run.html

出力は plotly.js を埋め込んだ自己完結の単一 HTML ファイル
(CDN 参照は supply chain attack リスクのため不採用、常に inline 固定)。
x 軸はログ先頭からの相対秒。4 段サブプロットが x 軸を共有するので、
ズーム/パンが全段リンクし、hover は同時刻の全値を一括表示する。
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

# temp_c のサニチネル。NTC 開放/フルスケール時に engine 側が返す値。
# (telemetry.hpp to_temp() が adc>=FS で -273.15 を返す)
TEMP_SENTINEL = -273.0  # これ以下は欠測扱いで NaN 化し、温度軸を汚さない

# (列名, 凡例ラベル, 線色) の段構成。色は plotly デフォルト寄せ。
DRIVE = [("l", "L", "#2ca02c"), ("r", "R", "#d62728")]
CURRENT = [
    ("i_esc_1", "ESC#1", "#1f77b4"),
    ("i_esc_2", "ESC#2", "#ff7f0e"),
    ("i_sys", "SYS", "#9467bd"),
]
VOLTAGE = [
    ("v_cell_1", "CELL#1", "#1f77b4"),
    ("v_bat", "BAT", "#ff7f0e"),
    ("v_q", "Q", "#9467bd"),
]
TEMP = [("temp_c", "TEMP", "#e377c2")]


def load(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    if "t_unix_ms" not in df.columns:
        raise ValueError(f"{path}: 't_unix_ms' 列が無い (CSV スキーマ不一致)")
    if df.empty:
        raise ValueError(f"{path}: データ行が無い (ヘッダのみ)")

    # 相対秒 (先頭=0) を主 x 軸に。絶対時刻は wall_clock で別途保持。
    t0 = df["t_unix_ms"].iloc[0]
    df["t_rel_s"] = (df["t_unix_ms"] - t0) / 1000.0
    df["wall_clock"] = pd.to_datetime(df["t_unix_ms"], unit="ms")

    # サニチネル温度を欠測化 (プロットで線が切れる = 正しい挙動)
    if "temp_c" in df.columns:
        df.loc[df["temp_c"] <= TEMP_SENTINEL, "temp_c"] = pd.NA

    return df


def add_traces(fig: go.Figure, df: pd.DataFrame, specs, row: int) -> None:
    for col, label, color in specs:
        if col not in df.columns:
            continue
        fig.add_trace(
            go.Scatter(
                x=df["t_rel_s"],
                y=df[col],
                name=label,
                mode="lines",
                line=dict(color=color, width=1.5),
                legendgroup=f"row{row}",
                legendgrouptitle_text=None,
            ),
            row=row,
            col=1,
        )


def build(df: pd.DataFrame, title: str) -> go.Figure:
    fig = make_subplots(
        rows=4,
        cols=1,
        shared_xaxes=True,
        vertical_spacing=0.04,
        row_heights=[0.22, 0.28, 0.28, 0.22],
        subplot_titles=(
            "Drive command (l / r)",
            "Current (A)",
            "Voltage (V)",
            "Temp (°C)",
        ),
    )

    add_traces(fig, df, DRIVE, row=1)
    add_traces(fig, df, CURRENT, row=2)
    add_traces(fig, df, VOLTAGE, row=3)
    add_traces(fig, df, TEMP, row=4)

    fig.update_yaxes(title_text="cmd", range=[-520, 520], row=1, col=1)
    fig.update_yaxes(title_text="A", row=2, col=1)
    fig.update_yaxes(title_text="V", row=3, col=1)
    fig.update_yaxes(title_text="°C", row=4, col=1)
    fig.update_xaxes(title_text="t (s, ログ先頭基準)", row=4, col=1)

    dur = df["t_rel_s"].iloc[-1]
    start = df["wall_clock"].iloc[0]
    fig.update_layout(
        title=f"{title}  —  {len(df)} rows / {dur:.1f}s @ {start:%Y-%m-%d %H:%M:%S}",
        hovermode="x unified",  # 同時刻の全段の値を一括表示
        legend=dict(groupclick="toggleitem"),
        template="plotly_dark",
        margin=dict(l=70, r=30, t=70, b=50),
        height=900,
    )
    # スパイクライン: 縦線でカーソル時刻を全段に通す
    fig.update_xaxes(showspikes=True, spikemode="across", spikethickness=1)

    return fig


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="pippitank telemetry CSV -> interactive HTML")
    ap.add_argument("csv", type=Path, help="入力 CSV (pippitank-ui --log の出力)")
    ap.add_argument("--out", type=Path, default=None, help="出力 HTML (既定: 入力名 .html)")
    args = ap.parse_args(argv)

    out = args.out or args.csv.with_suffix(".html")

    try:
        df = load(args.csv)
    except (OSError, ValueError) as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        return 1

    fig = build(df, args.csv.name)
    # CDN 参照は supply chain attack リスクのため不採用、plotly.js を埋め込む
    fig.write_html(out, include_plotlyjs="inline", full_html=True)
    print(f"[INFO] wrote {out}  ({len(df)} rows)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
