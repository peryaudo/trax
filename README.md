# Trax

## 短期目標

- [ ] NegaMax<LeafAverageEvaluator>(depth=1)より強くしていく

## 状況

* モンテカルロを実装した
  * NegaMax<MonteCarloEvaluator>(depth=1) ＞ Simple<LeafAverageEvaluator>くらいはある
* EdgeColorEvaluator実装したけど効果は今ひとつ
  * ちゃんとdepth=3＞depth=2になってるのはえらいけどdepth=4は3より弱い・・・
* 評価関数がスケールしない
* フレームワーク部がいま一つ遅い気がする（毎ステップnewしてるのを含む）
  * StateInfoの導入とDoMoveの差分更新
  * FillForcedPiecesのDFS実装

## ToDo

GPS将棋の人のスライドを見ると（＋NM<MCE>(depth=3) vs NM<LAE>(depth=2)戦とかを見てると）
やっぱりモンテカルロ探索はあんまりうまくいかない戦略なのかなと思う。
UCTは楽しそうなのでそのうち実装してみてもいいけど、

Timer(int timeout_ms=800)
bool Timer::Check()
void Timer::IncrementNodeCounter()
int Timer::nps()


### やるべき

* TranspositoinTable版のPerftを実装
* 普通の探索部のほうにnpsカウンタの実装
  http://d.hatena.ne.jp/LS3600/20090919/1253319013
* floodgate風レーティングをつけてくれるStartTournamentを実装
  トーナメントをしてRを出すコードを書かないといけない…

### どうでもいい

* 反復深化と時間制限の実装・マルチスレッド化→スケールする評価関数が作れてからね…
  * 秒数制限を守るのに使うような関数群をSearcherのベースクラス作ってそこに書いておく
    それでどのSearcherも共通ルーチン使ってできるように
    n回ループの内側きたらnanosec取得
    前回からの時間で次のnの値を調整
    0.8秒すぎたら店じまい
  * NegaMaxに反復深化入れる
  * マルチスレッド対応
* モンテカルロ木・UCT実装してみる
* 大会用デーモンで動くこと確認

## ネタ


### 評価関数につかえそうなもの

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

### Positionの高速化？

→ここがボトルネックとは限らない。あくまでアイデアとして。

やるなら別ブランチ作って作業しましょうエンバグが怖いんで

Position、DoMoveでは新しいオブジェクトを生成するしかないと思っていたが、
案外普通にDoMove, UndoMove, StateInfo体制できそう

最初からある程度でかめのboard_を確保して、ForcedPlayが起こったマスの座標をStateInfoで覚える
ForcedPlayが起こるマスが1ステップあたり64は大体ぜってーこえないため
超えた時のためにStateInfoにLinkedList的に次のStateInfoEntry?みたいなヤツへのポインタはつけとく。そっちは普通にnewしてデストラクタではdeleteする。けど普段は絶対つかわれないって算段。

64回ループは板完コピより明らかに速いし実際絶対そんなでかくない。
red_winner, white_winnerはStateInfoに入れておけばよい。
コールスタックに64バイト積むだけなのでPositionの比ではない

でかめの板をはみ出したら、そこではさらにでかい板を確保して、そこに全部コピーしてから再開する
戻る時は縮小しない

やるとしてもmargin_みたいな変数を管理してat()内で足さないといけないのだけが難点だな…

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


## ぼくのせんりゃく（古）


ある方向から置くと連鎖させられてある方向から置くと連鎖させられないやつとかある

外側に向いてる自分の色の面とか評価関数にできるな

GPUはDeep Learning使わない限り使い所ないなあ

しかも肝心のモンテカルロ木探索はTrax向きではないみたいだし…

案外囲碁AIよりも将棋AIのアプローチを真似たほうがいいっぽい？

評価関数を作る場合囲碁とかリバーシってどういう特徴量を使ってるんだろう？Traxは案外使うべき特徴量というのは存在はするらしい(Threatの数)

そもそもいろんな意味でゲームの構成が分かってない

代替何手ぐらいでゲーム終了するのかとか

n手先までの状態数がどれくらいに膨れ上がるのかとか

このへんの特性が分からないと戦略の立てようがない

さらに、棋譜が全然手に入らないという特徴もある

わりとやねうら王みたいな感じでフレームワーク部を切り出して複数戦略書いて自己対戦させて…とかしたほうがいいのか…？

GPS将棋マンによるTrax評
http://www.slideshare.net/shogotakeuchi/ss-62415546

合法手が増えてくって言ってるけど、Traxも案外連鎖があるので一定？
というか、違う合法手に見えて実は本当に同じ手というのが存在しうる。
という作者の誤解がもしあれば、アルファベータ探索が案外いけるハズ。

この辺の数字間隔がつかめねえ〜

あとマスがでかくなってくのをどう管理すればいいかもどうZobrist Hashingすればいいかも分からないし

* MinMax探索型なのか
* MCTS型なのか
* あるいはこの2つを混ぜられるもんなのか



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