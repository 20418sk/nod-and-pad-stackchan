# ESP-IDF公式顔検出・独立診断

## 目的

現行の`stackchan_k151` Arduino版と完全に分離し、GC0308画像をEspressif公式ESP-DL Human Face Detectへ入力して、顔の有無・矩形・信頼度・横中心だけをUSBシリアルへ表示します。画面、マイク、頭部タッチ、サーボ、Wi-Fi、BLE、microSD、音声機能は初期化しません。

この診断は追尾ファームではありません。サーボ指令は1行も含まず、実機精度と処理時間を確認するためだけの環境です。

## ビルド

通常版とは別の設定ファイルを明示します。

```powershell
pio run -c platformio-face-idf.ini -e stackchan_k151_face_diag
```

ビルド時には公式ESPDet Pico 224モデルを`human_face_detect.espdl`へ自動梱包し、
16MB Flash内の読み取り専用`human_face_det`パーティション用イメージとして生成します。
ファーム本体だけでなく、このモデルイメージも揃って初めて診断版として動作します。
診断版の成果物は通常版と競合しない`.pio/face-idf-build`へ分離しています。

通常版は従来どおり次でビルドできます。

```powershell
pio run -e stackchan_k151
```

## 書き込み

自動では書き込みません。診断版を書き込む場合だけ、台座側USB-Cのポートを指定して手動で実行します。

```powershell
pio run -c platformio-face-idf.ini -e stackchan_k151_face_diag -t upload --upload-port COM5
```

診断版は現行Arduino版を上書きします。元へ戻すには通常版を再度書き込む必要があります。

## シリアル出力

顔なし:

```text
#12 fmt=YUYV thr=0.50 Y=112 C=186 FACE:none count=0 infer=45ms
```

顔あり:

```text
#13 fmt=YUYV thr=0.30 Y=108 C=181 FACE:L score=0.91 x=-28% box=[30,44,126,181] count=1 infer=52ms
```

`FACE:L/C/R`は未補正のカメラ画像座標です。実機の左右対応は、顔を左右へ動かして確認するまでサーボ方向として扱いません。

`fmt=YUYV`はGC0308の正しいY-Cb-Y-Cr配列、`thr=0.50`は公式標準しきい値です。従来のMSR+MNPでは低しきい値時に誤検出だけが発生したため、現在はESPDet Pico 224を評価します。`Y`は平均明るさ、`C`はサンプル内の最大・最小輝度差です。

## プライバシー

- フレームはPSRAM上のカメラバッファから直接推論し、直後に返します。
- Flash、NVS、microSDへ画像を書きません。
- Wi-Fi、BLE、HTTP、クラウドを初期化しません。
- シリアルへ出すのは顔矩形、信頼度、中心位置、処理時間だけです。
- 顔認識・個人識別・顔画像登録は行いません。
