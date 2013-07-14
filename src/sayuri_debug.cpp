/* 
   sayuri_debug.h: Sayuriをデバッグする。
   Copyright (c) 2013 Ishibashi Hironori

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to
   deal in the Software without restriction, including without limitation the
   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
   sell copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.
*/

#include "sayuri_debug.h"

#include <iostream>
#include <utility>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include "chess_def.h"
#include "chess_util.h"

#include "sayuri.h"

namespace Sayuri {
  /**************************/
  /* デバッグ用メイン関数。 */
  /**************************/
  // ================================================================
  int DebugMain(int argc, char* argv[]) {
    TranspositionTable* table = new TranspositionTable(1);

    HashKey pos_key = GenPseudoHashKey();
    int level = 3;
    int depth = 5;
    Side to_move = WHITE;
    int value = 200;
    TTValueFlag value_flag = TTValueFlag::EXACT;
    Move best_move;
    best_move.piece_square_ = E2;
    best_move.goal_square_ = E4;

    table->Add(pos_key, level, depth, to_move, value, value_flag, best_move);

    const TTEntry* entry_ptr =
    table->GetFulfiledEntry(pos_key, level, depth, to_move);

    TranspositionTable* table2 = new TranspositionTable(std::move(*table));

    const TTEntry* entry_ptr2 =
    table2->GetFulfiledEntry(pos_key, level, depth, to_move);

    if (entry_ptr == entry_ptr2) {
      std::cout << "True" << std::endl;
    } else {
      std::cout << "False" << std::endl;
    }

    delete table;
    delete table2;
    return 0;
  }
  // ================================================================

  // アサートする。
  void Assert(bool expr) {
    if (expr) return;
    throw SayuriError("Sayuriエラー: アサート失敗。");
  }

  // 擬似ハッシュキー生成。
  HashKey GenPseudoHashKey() {
    int time = std::time(nullptr);
    std::mt19937 engine(time);
    std::uniform_int_distribution<HashKey> dist(0, 0xffffffffffffffffULL);
    return dist(engine);
  }

  // ビットボードを出力する。
  void PrintBitboard(Bitboard bitboard) {
    // 行間のボーダー。
    static const char* border = " +---+---+---+---+---+---+---+---+";

    //一番上のボーダーを出力。 
    std::cout << border << std::endl;

    char line[35];  // 行の文字列。
    int index;  // 行の文字列のインデックス。
    char line_num = '8';  // 行番号の文字。
    Bitboard point = 0x1ULL << 56;  // 調べるビットボードの位置。
    for (int i = 0; i < 8; i++) {
      // 行の文字列の配列を初期化。
      std::memset(line, '\0', sizeof(char) * 35);
      // 行番号を入れる。
      line[0] = line_num--;
      // 行番号の右側の境界を入れる。
      line[1] = '|';
      // 行の文字列のインデックスを初期化。
      index = 2;
      // ビットを調べて文字列に文字を入れる。
      for (int j = 0; j < 8; j++) {
        // ビットが立っている場合、文字を入れる。なければ空白。
        if (bitboard & point) {
          line[index++] = '(';
          line[index++] = '+';
          line[index++] = ')';
          line[index++] = '|';
        } else {
          line[index++] = ' ';
          line[index++] = ' ';
          line[index++] = ' ';
          line[index++] = '|';
        }
        if (j != 7) point <<= 1;
      }

      // 一つ下のランクに落とす。
      point >>= 15;

      // 行を出力。
      std::cout << line << std::endl;
      // ボーダーを出力。
      std::cout << border << std::endl;
    }

    // 最後の行のファイル名を出力。
    std::cout << "   a   b   c   d   e   f   g   h" << std::endl;
  }

  // 手を出力する。
  void PrintMove(Move move) {
    // ファイルとランクの文字の配列。
    constexpr char fyle_array[NUM_FYLES] = {
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
    };
    constexpr char rank_array[NUM_RANKS] = {
      '1', '2', '3', '4', '5', '6', '7', '8'
    };

    // ファイルとランク。
    Fyle fyle;
    Rank rank;

    // 動かす駒の位置を出力する。
    std::cout << "Piece: ";
    fyle = Util::GetFyle(move.piece_square_);
    rank = Util::GetRank(move.piece_square_);
    std::cout << fyle_array[fyle] << rank_array[rank] << std::endl;

    // 移動先の位置を出力する。
    std::cout << "Goal: ";
    fyle = Util::GetFyle(move.goal_square_);
    rank = Util::GetRank(move.goal_square_);
    std::cout << fyle_array[fyle] << rank_array[rank] << std::endl;

    // 取った駒の種類を出力する。
    std::cout << "Captured Piece: ";
    switch (move.captured_piece_) {
      case PAWN:
        std::cout << "Pawn";
        break;
      case KNIGHT:
        std::cout << "Knight";
        break;
      case BISHOP:
        std::cout << "Bishop";
        break;
      case ROOK:
        std::cout << "Rook";
        break;
      case QUEEN:
        std::cout << "Queen";
        break;
      case KING:
        std::cout << "King";
        break;
      default:
        Assert(false);
        break;
    }
    std::cout << std::endl;

    // 昇格する駒の種類を出力する。
    std::cout << "Promotion: ";
    switch (move.promotion_) {
      case PAWN:
        std::cout << "Pawn";
        break;
      case KNIGHT:
        std::cout << "Knight";
        break;
      case BISHOP:
        std::cout << "Bishop";
        break;
      case ROOK:
        std::cout << "Rook";
        break;
      case QUEEN:
        std::cout << "Queen";
        break;
      case KING:
        std::cout << "King";
        break;
      default:
        Assert(false);
        break;
    }
    std::cout << std::endl;

    // キャスリングを出力する。
    Castling castling = move.last_castling_rights_;
    std::cout << "<Last Castling Rights>" << std::endl;
    if (castling & WHITE_SHORT_CASTLING)
      std::cout << "  White Short Castling" << std::endl;
    if (castling & WHITE_LONG_CASTLING)
      std::cout << "  White Long Castling" << std::endl;
    if (castling & BLACK_SHORT_CASTLING)
      std::cout << "  Black Short Castling" << std::endl;
    if (castling & BLACK_LONG_CASTLING)
      std::cout << "  Black Long Castling" << std::endl;

    // アンパッサンできるかどうかを出力する。
    bool can_en_passant = move.last_can_en_passant_;
    std::cout << "Last Can En Passant: ";
    if (can_en_passant) std::cout << "True";
    else std::cout << "False";
    std::cout << std::endl;

    // アンパッサンのターゲットを出力する。
    Square en_passant_target = move.last_en_passant_target_;
    fyle = Util::GetFyle(en_passant_target);
    rank = Util::GetRank(en_passant_target);
    std::cout << "Last En Passant Target: "
    << fyle_array[fyle] << rank_array[rank] << std::endl;

    // 手の種類を出力する。
    std::cout << "Move Type: ";
    switch (move.move_type_) {
      case NORMAL:
        std::cout << "Normal";
        break;
      case CASTLING:
        std::cout << "Castling";
        break;
      case EN_PASSANT:
        std::cout << "En Passant";
        break;
      case NULL_MOVE:
        std::cout << "Null Move";
        break;
      default:
        Assert(false);
        break;
    }
    std::cout << std::endl;
  }

  /**********************
   * ストップウォッチ。 *
   **********************/
  namespace {
    std::time_t start_time = 0;  // スタートタイム。
    std::time_t end_time = 0;  // ストップタイム。
  }
  // ストップウォッチをスタートする。
  void Start() {
    std::time(&start_time);
  }
  // ストップウォッチをストップする。
  void Stop() {
    std::time(&end_time);
  }
  // ストップウォッチで計測した秒数を得る。
  double GetTime() {
    return std::difftime(end_time, start_time);
  }
}  // namespace Sayuri
