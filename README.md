# Trax

Trax AI、雰囲気としては、オセロAIが一番近いと思ってるんだよなあ。

均一なピース。
ゲーム木が末広がりであり、αβ探索向き。
だけど将棋のKPPのように比較的自明に見えてくる評価関数のフィーチャというものが無い。
一見モンテカルロ木探索向けに見えるが、ゲーム木の形状上αβ探索のほうが強い。

ということから類推すると、盤面をそのままニューラルネットワークにぶち込むより、
logistelloのようにパターンを見出して、線形回帰したほうが強いということになる。

## 中期目標

- [ ] NegaMax<LeafAverageEvaluator>(depth=1)より強い評価関数を作る

## 短期目標

- [ ] 周辺ツール類を整備する
- [ ] 他ゲームの評価関数の手法をよくよく検討する
- [ ] 考えた評価関数を少しずつ実証していく

## 状況

* NegaMax<LeafAverageEvaluator>(depth=1)がむちゃくちゃ強くて全然超えられない。
* 周辺ツールの整備と他AIとの対戦などの検証とともにKPP風評価関数を実装する

## ToDo

### やるべき

* 自己対戦で生成された棋譜と外から読み込んだ棋譜を区別せずに扱えるデータ構造があるといいなあ

* 大体他ゲーが平均何手で終局するかも調べてTraxについて序盤中盤終盤を定義する
* PerftにTranspositoinTableを入れる。TranspositoinTableをクラスに分離する。

* Lobbybotと戦わせてみる。 http://www.traxgame.com/shop_download.php

* タイマ実装
  Timer(int timeout_ms=800) -1でCheckが常にfalseを返すように これを実行した時間を起点にする。
  bool Timer::Check() 時間を過ぎたらtrueを返す
  void Timer::IncrementNodeCounter() Searchの基底部て呼ぶ
  int Timer::nps()

* TranspositoinTable版のPerftを実装
* 普通の探索部のほうにnpsカウンタの実装
  http://d.hatena.ne.jp/LS3600/20090919/1253319013
* floodgate風レーティングをつけてくれるStartTournamentを実装
  トーナメントをしてRを出すコードを書かないといけない…

* --best-move
  (computer playerの白赤)
  N
  1番目のtrax notation
  ...
  n番目の,,

  をstdinから取ってstdoutにbest moveのtrax notationだけを返すモードを実装する

* で、golangでtrax-daemon実装してwebで遊べるようにする
  * ユーザーは毎回上のこれまでの棋譜をサーバーに送信

* 多分定石も入れたほうがいいんだろうな…

* Evaluateのインターフェイス自体を差分更新に対応させることは可能？

* 大会用デーモンで動くこと確認

### 今はどうでもいい

* フレームワーク部がいま一つ遅い気がする
  * と思ったけど将棋とかに比べると単純な分普通にnps出てる。まだnps出せるのはいいことだけど…
  * StateInfoの導入とDoMoveの差分更新
  * FillForcedPiecesのDFS実装
* 反復深化と時間制限の実装・マルチスレッド化→スケールする評価関数が作れてからね…
  * 秒数制限を守るのに使うような関数群をSearcherのベースクラス作ってそこに書いておく
    それでどのSearcherも共通ルーチン使ってできるように
    n回ループの内側きたらnanosec取得
    前回からの時間で次のnの値を調整
    0.8秒すぎたら店じまい
  * NegaMaxに反復深化入れる
  * マルチスレッド対応
* モンテカルロ木・UCT実装してみる

* （df-pnについて勉強する？→全然強くならないらしい http://yaneuraou.yaneu.com/2014/12/14/%E3%82%84%E3%81%AD%E3%81%86%E3%82%89%E7%8E%8B%E3%81%AE%E9%96%8B%E7%99%BA%E3%81%AE%E6%AD%A9%E3%81%BF2014%E5%B9%B4%E3%81%BE%E3%81%A7/）

* 静止探索ってなに？

* @0+ A2+ @1+ B0/ B0/ B0/ A1/ @1/ A0/
  NegaMax-NegaMaxでよくこけるパターン→水平線効果そのもの

## 評価関数の設計

ラインごとに分離して、スコアリングし、それを足す（将棋のKPPみたいな感じ）。

* 実は、端の座標と方向（、と盤面の形状）のみが重要という説はある
* あるいは、ビクトリーラインの評価関数とループの評価関数を分離するというのも考えられる

ラインの評価関数は、
* 端点間から端点までの外周距離（外側にピースを置いていって何ピースで繋げるか）
  * マンハッタン距離で近似可能
* 端点から端点までの長さ
* 端点接している2面が正反対向きか
* 端点接している2面が同じ向きか
* こういうのを定義して、 
   <-------->
      /------\ A
      \-\      |
  /-----/      V
  * その長さ(の最大値？)
    (これらはバグってる版のTraceVictoryLineOrLoopが抽出していた特徴を意識)
などを特徴量に使える。これらの係数を将棋のように学習させる。


あるいは、これ自体をディープラーニングで学習させる発展も考えられる。
線を差分列にして、正規化し、RNNやLSTMを使って形の「良さ」を学習させる。

すべての線に同じ係数を適用しながら学習させていくわけで、ある種の畳み込みとも考えられる

ラインの総数が盤面によって違うので、どうそこを正規化するのか、
後半になるにつれて値が大きくなってしまっていいのか問題を解決しないといけない？
ノンタッチでもどうにかなる？

はい、でですね
勝率=f(特徴)なる関数を学習させるとします

線形和にしとけば、w_1(ライン1の特徴1 + ライン2の特徴2 ...) + w_1 (..) ...
みたいになって依然普通に最小二乗法の最急降下法できる

// あるいはw_1(... )の...の中をプーリング層を意識して、最大値か最小値のみ取るようにしちゃっても良い？


元の、左右が繋がっている赤の本数、みたいな因子を拾い取れる設計にしないとね
でも今拾えてる気がする。

本数で割ったほうが良いのかなあよくわからん


## 資料

* 強くなりそうな評価関数の特徴量を他のゲームを参考にもっとちゃんと考える
  * このへんを全部読む
  http://sealsoft.jp/thell/learning.pdf
  https://skatgame.net/mburo/ps/improve.pdf
  https://skatgame.net/mburo/ps/glem.pdf
  http://www.kitsunemimi.org/vsotha/algorithm.html
  http://d.hatena.ne.jp/LS3600/archive
  http://d.hatena.ne.jp/hiyokoshogi/archive
  http://yaneuraou.yaneu.com/
  http://yaneuraou.yaneu.com/yaneuraou_mini/
  http://yaneuraou.yaneu.com/stockfish%E5%AE%8C%E5%85%A8%E8%A7%A3%E6%9E%90/
  http://link.springer.com/chapter/10.1007%2F978-3-540-40031-8_2#page-1

  http://shogo82148.github.io/homepage/memo/algorithm/least-squares-method/

  http://www.logos.ic.i.u-tokyo.ac.jp/~yano/PDF/evo-bonanza.pdf

  http://yaneuraou.yaneu.com/2015/11/19/%E4%BD%95%E6%95%85%E3%80%81%E6%BF%80%E6%8C%87%E3%81%AFsgd%E3%82%92%E4%BD%BF%E3%82%8F%E3%81%AA%E3%81%8B%E3%81%A3%E3%81%9F%E3%81%AE%E3%81%A7%E3%81%99%E3%81%8B%EF%BC%9F/

  オセロのPerft: http://www.aartbik.com/MISC/reversi.html

  オセロ、案外手数増えないな…そりゃ深く読めるわけだ
  Traxは手数は増えすぎなんだけど、局所性が強いはずなので盤面4分割とかを使えば相当探索をサボれるはず。
  というのがオセロ/チェス/将棋/囲碁?とかと異なる良い性質

  将棋に比べると全然浅くしか読めないけどそもそも人間の強い人達のプレイも将棋に比べるとはるかに速く終局する



## ネタ

パスベースで盤面持つとか言ってる論文が多いけど盤面舐めるだけで大量の分岐ミス吐いていくって正直やばすぎなんだよなあ
ぜってー効率的じゃないと思うんだけど………
ひろいちだいのヤツは評価すら取ってないから論外としてテヘラン大のヤツもmapベース実装と比較してそうだしなあ



### 評価関数につかえそうなもの（古）

中盤が弱いはず。序盤と終盤は強いはずだけど終盤は読みが足らないかも
→詰めルーチン的なのがあるとやっぱりいいのかも

オセロの例:
https://skatgame.net/mburo/ps/glem.pdf

  * 端から端を繋いでる自分の色のラインの数
    * 例えば左右をつなぐ白い色の線が何本あるかとか

  * それを置いたことでの盤面のサイズ
    * Evidence: 探索空間を狭くするほうに誘導できるので探索速度が上がり有利になる
  * 埋まっているマス目の数
    * Evidence: 連鎖をより起こすものを置いたほうがよい？ or 置かないほうがよい？
  * 探索の深さ→とりづらいので埋まっているマス目の数で代用？

  * モンテカルロ
  * モンテカルロ木探索
  * 盤面を飲み込ませたCNN

どう実装するか分からないけど、探索時に盤面をdecomposeするっていうのはアリだよね
あるいはあるNegaMax()についてGenerateMovesを一定の領域内だけの手について探索する
（たとえば下方向の手のみを検討するとか）

http://www.traxgame.com/about_strategy.php#16

やっぱりLeafAverageEvaluatorがこれ以上スケールしないのか？

一応SimpleSearcher < NegaMax(depth=1)は達成できててこれは旧版のNegaMax(depth=2)相当なのでまあ深くはできてる

6/10朝版のdepth=2のほうがたしかに強かったのだけれども、TraceVictoryLineOrLoop()
が普通にバグってたから何がどうだったのか正直わかんねー

一つ考えられる仮説としてはバグってた版のTraceVictoryLineOrLoop()が何らかの

特徴を抽出してたというのはありそう

その証拠にやたらクネクネした局面が生成されてた

でもそれって別ゲーだよね…

バカな指し手を追ってみるのはいいと思う

楽しくやるのが第一なんであんまりbisectとかしないように。

まあ深く探索しすぎて弱くなるさもありなんだと思うんだよな〜

深読みしても有益な情報が出てこない評価関数というのはありそう

評価関数が何をやってるかを考えるべき
統計とってないけどLeafAverageEvaluatorは簡単なループが作れるだけで、
SimpleSearcherだと簡単なループ防衛すら危うくなるからdepth=1はつよいけど
これ以上深くしてもメリットがないってことだと思う
Dumpの下に評価関数を書くってテク

LeafAverageEvaluatorは序盤向けの評価関数だと言える

* NoneEvaluatorでdepth=5>4>3>2>1が成り立っていることを検証し、
  深くすると弱くなる現象がLeafAverageEvaluatorのヒューリスティクス由来であることを示す
  -> That was true. But depth>=4 is impractical due to its huge memory footprint (more than 8GB),
     and also negamax1-la is stronger than negamax4-na actually!

### Positionの高速化？

→ここがボトルネックとは限らない。あくまでアイデアとして。

やるなら別ブランチ作って作業しましょうエンバグが怖いんで

Position、DoMoveでは新しいオブジェクトを生成するしかないと思っていたが、
案外普通にDoMove, UndoMove, StateInfo体制できそう

* StateInfoの導入とDoMoveの差分更新
* FillForcedPiecesのDFS実装

最初からある程度でかめのboard_を確保して、ForcedPlayが起こったマスの座標をStateInfoで覚える
ForcedPlayが起こるマスが1ステップあたり64は大体ぜってーこえないため
超えた時のためにStateInfoにLinkedList的に次のStateInfoEntry?みたいなヤツへのポインタはつけとく。そっちは普通にnewしてデストラクタではdeleteする。けど普段は絶対つかわれないって算段。

64回ループは板完コピより明らかに速いし実際絶対そんなでかくない。
red_winner, white_winnerはStateInfoに入れておけばよい。
コールスタックに64バイト積むだけなのでPositionの比ではない

でかめの板をはみ出したら、そこではさらにでかい板を確保して、そこに全部コピーしてから再開する
戻る時は縮小しない


やるとしてもmargin_みたいな変数を管理してat()内で足さないといけないのだけが難点だな…


さらに、同様に合法手の生成もプログレッシブにできる。
これで合法手生成のコストがO(盤面サイズ)からO(log 合法手)に落ちる。
ってかこれは簡単だけど盤面コピーコストのほうが絶対でかいからやってなかっただけ
今でもオーダー上はライントレースのほうがコスト高だし実際やってみないとわからん

### 置換表

以下の様なことを考えた、が、盤面反転とか実際にそれで省けるケースが今ひとつ多そうじゃないという事に気がついた。

* 置換表のデザインはどこが悪いのか？
* 置換表のconflictを無視すると普通にむっちゃ弱くなるから無視してはいけない
* 反転と回転をHash()の引数でできるようにする
  * 反転も左右反転と色反転、手番反転などアリ

手番まで格納するのは非常にばかばかしい
統一して例えば全て白になったつもりで格納するほうがよい

ローリングハッシュの質は難ありなのでやっぱりZobristHashingしたい

工夫すればやっぱりZobrist Hashingできそうだけど………

http://hos.ac/blog/#blog0003

http://algoogle.hadrori.jp/algorithm/rolling-hash.html

* 簡単なビット操作で回転と色反転をできる並べ方とかないか？
  あと2次元配列を最短コードで回転させる方法

### 勝利判定 or TraceVictoryLineOrLoop

* パフォーマンス心配だしバグりやすいのでprogressive版とそうでない版を両方実装して試したい…
* 終局判定が不安すぎるから公式の棋譜データ流し込んで全部終局扱いにちゃんとなるかチェックしたい

* 下の手法は現行より効率的なはずだが、さらにprogressiveに更新可能にはできないか検討する。
  例えばUnionFind木が使えないか？

以下非progressive版についての話

すべてのトラックについて番号付けする。空でない全座標について、
［座］［標］［上下左右辺］ = 番号みたいな配列をdfsで作る。

それが終わったらVictory LineとLoopを判定する。

* Loop
  new bool[線の数]を確保する。
  広義外接上をめぐって、登場したら[線番号]=trueを塗る。
  登場しないものがあったら、その色は勝ち
  ただし複数存在して引き分けもありうるので中断はできない

* Victory Line
  両面が8より小さいのサイズの盤面なら以下の手順を省略できる。
  new int[線の数]を確保する。
  狭義外周上をめぐって[線番号] |= 1 << その面 を塗る
  1010や0101があったら、その色は勝ち
  ただし複数存在して引き分けもありうるので中断はできない

上記で勝利判定が出たらはじめて盤面をたどり直して、勝ったのが赤か白かを判定できる。

ヒットした線番号一覧はさすがにvectorに入れても大丈夫だと信じている。

### モンテカルロについて

モンテカルロ法自体をこれ以上いじるのはやめて、いい評価関数探しやったほうがいい気がする。

GPS将棋の人のスライドを見ると（＋NM<MCE>(depth=3) vs NM<LAE>(depth=2)戦とかを見てると）
やっぱりモンテカルロ探索はあんまりうまくいかない戦略なのかなと思う。

モンテカルロ+評価関数とかも試したけど全然よくなりませんでした。

GPS将棋マンによるTrax評
http://www.slideshare.net/shogotakeuchi/ss-62415546


モンテカルロとLeafAverageEvaluatorの組み合わせ、LeafAverageEvaluatorがかなりクオリティの高いスコアを提供してくれて、
水平線効果が起こってるときのみモンテカルロの出番がくるから、絶対にLeafAverageEvaluatorのスコアをつぶさないような重み、例えば/ 10000した値とかをLeafAverageEvaluator==0の時に返せば強くなったりしないか

## ぼくのせんりゃく（古）


ある方向から置くと連鎖させられてある方向から置くと連鎖させられないやつとかある

GPUはDeep Learning使わない限り使い所ないなあ

案外囲碁AIよりも将棋AIのアプローチを真似たほうがいいっぽい？

評価関数を作る場合囲碁とかリバーシってどういう特徴量を使ってるんだろう？Traxは案外使うべき特徴量というのは存在はするらしい(Threatの数)

そもそもいろんな意味でゲームの構成が分かってない

代替何手ぐらいでゲーム終了するのかとか

n手先までの状態数がどれくらいに膨れ上がるのかとか

このへんの特性が分からないと戦略の立てようがない

さらに、棋譜が全然手に入らないという特徴もある

わりとやねうら王みたいな感じでフレームワーク部を切り出して複数戦略書いて自己対戦させて…とかしたほうがいいのか…？

Deep Learningで評価関数作ってMinMax案外正しい気もするけど確実に手で作れる評価関数以上のものをアレできる必要があるんだよな

制限時間1秒なる

GnuTraxは大体思いつく限りのナイーブなアルゴリズムが全部実装されててすごい。8x8Traxだけど………

https://github.com/MartinMSPedersen/GnuTrax/blob/master/src/org/traxgame/main/ComputerPlayerAlphaBeta.java

GnuTrax、CornersとThreatsの数をアルファベータの評価関数にしてるけど、序盤ではこの評価関数じゃ何も見えなさそうなのと、Threatsが全部リストアップされてるのがいただけない。
というかCornersとThreatsが分けられてるの自体がいただけない…

このThreats databaseとかcornersみたいなのをDeep Learningに感じ取ってほしい。（ムリでは………Deep Learningに多くを求めすぎでは………）

複数の起点から全く同じ連鎖を起こすことができる。一番左上のみを有効な手として扱って他をエイリアスとすることで、Moveを正規化できる。でもあんまり意味ないかも。

連鎖とかがあるせいで差分更新・ロールバックと、zobrist hashが使えないのが最大の問題。
→zobrist hashingに関しては別に置くコマを全部xorすれば問題ないじゃん。一番の問題は座標が定まらないほうであって、連鎖ではない。手順前後を正しくハッシュ化できることが一番重要
まあコピーを発生させまくるのでzobristはどのみち後回し。rollbackできないのは変わらないし………
正規化+連鎖でどこで止まったかとか記録すればロールバックもできる気がしてきたけど複雑すぎてコピーより軽くなる気しないな…


http://www.kitsunemimi.org/vsotha/algorithm.html
https://skatgame.net/mburo/ps/improve.pdf

リファレンス実装は毎回ボード（しかもmap!）コピーしてる…
でもボードサイズも変化するし毎回コピーする以外どうしようもなさある

どのみち毎回全コピーする場合は配列の拡張は問題にならない

小さいサイズについて特殊化してフォールバックするようにするとかしか思いつかん

FPGA的な話。一応そのうち考えとく。

http://ieeexplore.ieee.org/xpl/login.jsp?tp=&arnumber=7377789&url=http%3A%2F%2Fieeexplore.ieee.org%2Fxpls%2Fabs_all.jsp%3Farnumber%3D7377789
http://ieeexplore.ieee.org/xpl/login.jsp?tp=&arnumber=1393254&url=http%3A%2F%2Fieeexplore.ieee.org%2Fxpls%2Fabs_all.jsp%3Farnumber%3D1393254
http://link.springer.com/chapter/10.1007%2F978-3-540-69812-8_108#page-1

棋譜が無いわけではない

http://www.traxgame.com/games_archives.php

これの棋譜を与えて正しく表示されることを確認する。
http://www.traxgame.com/games_archives.php?pid=162

## 手候補・連鎖の判定

二次元boolで候補を持ってプログレッシブに更新できる？
効果薄かも
