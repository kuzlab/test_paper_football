# M5Paper Football Scores Display

M5Paper（ESP32搭載電子ペーパーディスプレイ）を使用したサッカー試合結果表示システムです。football-data.org APIからリアルタイムで試合データを取得し、美しい横長レイアウトで表示します。

![M5Paper Football Display](https://img.shields.io/badge/M5Paper-ESP32-blue)
![Arduino](https://img.shields.io/badge/Arduino-IDE-orange)
![API](https://img.shields.io/badge/API-football--data.org-green)

## 🚀 特徴

- **リアルタイムデータ**: football-data.org APIから最新の試合結果を取得
- **美しい表示**: 電子ペーパーの特性を活かした横長レイアウト
- **直感的操作**: タッチ操作と物理ボタンの両方に対応
- **自動更新**: 30分ごとの自動データ更新
- **エラー耐性**: WiFi接続失敗時もモックデータで動作継続
- **多言語対応**: 日本語と英語のデバッグ情報

## 📋 必要なハードウェア

- **M5Paper** (ESP32搭載電子ペーパーディスプレイ)
- **WiFi接続** (インターネットアクセス必須)
- **USB-Cケーブル** (電源供給・アップロード用)

## 📚 必要なライブラリ

Arduino IDEのライブラリマネージャーから以下をインストールしてください：

- **M5Unified** - M5Stack統合ライブラリ
- **ArduinoJson** - JSON解析ライブラリ

## ⚙️ セットアップ

### 1. リポジトリのクローン
```bash
git clone https://github.com/kuzlab/test_paper_football.git
cd test_paper_football
```

### 2. 設定の変更
`test_paper_football.ino`を開き、以下の設定を環境に合わせて変更してください：

```cpp
// WiFi設定
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API設定（必要に応じて）
const char* apiKey = "da4aeda6d7b942ddbdeb727fda3059a2";
```

### 3. M5Paperへのアップロード
1. Arduino IDEで`test_paper_football.ino`を開く
2. ボード設定：
   - ボード: "M5Paper"
   - アップロード速度: 115200
3. アップロード実行

## 🎮 使用方法

### 起動
1. M5Paperの電源を入れる
2. WiFi接続が完了するまで待機（約30秒）
3. 初回データ取得が完了すると試合結果が表示されます

### 操作
- **物理ボタンC**: データ更新（リロード）
- **タッチ操作**: 右上の「Reload」ボタンをタッチ
- **自動更新**: 30分ごとに自動でデータ更新

### 表示内容
- **試合日付**: MM/DD (Day)形式
- **チーム名**: 最大10文字（超過時は省略）
- **スコア**: ホーム-アウェイ形式
- **勝敗表示**: ★（勝利）、=（引き分け）
- **大会名**: 略称で表示（EPL、La Liga等）

## 📊 表示例

```
┌─────────────────────────────────────────────────────────────┐
│                    FOOTBALL SCORES                          │
│  [Reload]                                                   │
│  Period: 06-10 to 06-19  Status: [OK] SUCCESS (Newest First)│
│  ────────────────────────────────────────────────────────── │
│  06/12 (Thu) ★Bragantino 0-3 Bahia        BSA              │
│  ────────────────────────────────────────────────────────── │
│  06/12 (Thu) =Vitória 0-0 Cruzeiro        BSA              │
│  ────────────────────────────────────────────────────────── │
│  06/13 (Fri) São Paulo 1-3 ★Vasco da Gama BSA              │
│  ────────────────────────────────────────────────────────── │
│  Touch 'Reload' or press button to refresh | Time: 15:30:45 │
└─────────────────────────────────────────────────────────────┘
```

## 🔧 トラブルシューティング

### WiFi接続エラー
- SSIDとパスワードが正しいか確認
- WiFiの電波状況を確認
- 接続失敗時はモックデータで動作継続

### API通信エラー
- インターネット接続を確認
- APIキーが有効か確認
- 通信失敗時はモックデータを使用

### 表示が更新されない
- 物理ボタンCまたはタッチボタンを押す
- 30分待って自動更新を確認
- 電源を再起動

## 📁 ファイル構成

```
test_paper_football/
├── test_paper_football.ino  # メインスケッチ
├── SPECIFICATION.md         # 詳細仕様書
└── README.md               # このファイル
```

## 🔗 使用API

- **football-data.org API v4**
  - 無料プランで利用可能
  - 10日間の日付範囲制限
  - レート制限あり

## 📝 ライセンス

このプロジェクトはMITライセンスの下で公開されています。

## 🤝 貢献

バグ報告や機能要望は[Issues](https://github.com/kuzlab/test_paper_football/issues)でお知らせください。

## 📞 サポート

- **GitHub Issues**: [https://github.com/kuzlab/test_paper_football/issues](https://github.com/kuzlab/test_paper_football/issues)
- **詳細仕様**: [SPECIFICATION.md](SPECIFICATION.md)

## 🎯 今後の予定

- [ ] 複数大会の選択機能
- [ ] より詳細な試合情報表示
- [ ] オフライン機能の強化
- [ ] 設定画面の追加
- [ ] 多言語対応の拡充

---

**注意**: このプロジェクトは教育・研究目的で作成されています。商用利用の際は適切なライセンス確認をお願いします。 