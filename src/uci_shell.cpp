/*
   uci_shell.cpp: UCIのチェスエンジン側インターフェイスの実装。

   The MIT License (MIT)

   Copyright (c) 2014 Hironori Ishibashi

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

#include "uci_shell.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <iterator>
#include <thread>
#include <chrono>
#include <sstream>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <functional>
#include "common.h"
#include "chess_engine.h"
#include "transposition_table.h"
#include "pv_line.h"
#include "fen.h"

namespace Sayuri {
  /**************************/
  /* コンストラクタと代入。 */
  /**************************/
  // コンストラクタ。
  UCIShell::UCIShell(ChessEngine& engine) :
  uci_command_(),
  engine_ptr_(&engine),
  table_ptr_(new TranspositionTable(UCI_MIN_TABLE_SIZE)),
  moves_to_search_(0),
  table_size_(UCI_DEFAULT_TABLE_SIZE),
  enable_pondering_(UCI_DEFAULT_PONDER),
  num_threads_(UCI_DEFAULT_THREADS),
  analyse_mode_(UCI_DEFAULT_ANALYSE_MODE),
  output_listeners_(0) {
    using namespace std::placeholders;

    // コマンドを登録する。
    // uciコマンド。
    std::vector<std::string> temp_1 {
      "uci"
    };
    uci_command_.Add("uci", temp_1,
    std::bind(&UCIShell::CommandUCI, this, _1));

    // isreadyコマンド。
    std::vector<std::string> temp_2 {
      "isready"
    };
    uci_command_.Add("isready", temp_2,
    std::bind(&UCIShell::CommandIsReady, this, _1));

    // setoptionコマンド。
    std::vector<std::string> temp_3 {
      "setoption", "name", "value"
    };
    uci_command_.Add("setoption", temp_3,
    std::bind(&UCIShell::CommandSetOption, this, _1));

    // ucinewgameコマンド。
    std::vector<std::string> temp_4 {
      "ucinewgame"
    };
    uci_command_.Add("ucinewgame", temp_4,
    std::bind(&UCIShell::CommandUCINewGame, this, _1));

    // positionコマンド。
    std::vector<std::string> temp_5 {
      "position", "startpos", "fen", "moves"
    };
    uci_command_.Add("position", temp_5,
    std::bind(&UCIShell::CommandPosition, this, _1));

    // goコマンド。
    std::vector<std::string> temp_6 {
      "go", "searchmoves", "ponder", "wtime", "btime", "winc", "binc",
      "movestogo", "depth", "nodes", "mate", "movetime", "infinite"
    };
    uci_command_.Add("go", temp_6,
    std::bind(&UCIShell::CommandGo, this, _1));

    // stopコマンド。
    std::vector<std::string> temp_7 {
      "stop"
    };
    uci_command_.Add("stop", temp_7,
    std::bind(&UCIShell::CommandStop, this, _1));

    // ponderhitコマンド。
    std::vector<std::string> temp_8 {
      "ponderhit"
    };
    uci_command_.Add("ponderhit", temp_8,
    std::bind(&UCIShell::CommandPonderHit, this, _1));
  }

  // コピーコンストラクタ。
  UCIShell::UCIShell(const UCIShell& shell) :
  uci_command_(shell.uci_command_),
  engine_ptr_(shell.engine_ptr_),
  table_ptr_(new TranspositionTable(*(shell.table_ptr_))),
  moves_to_search_(shell.moves_to_search_),
  table_size_(shell.table_size_),
  enable_pondering_(shell.enable_pondering_),
  num_threads_(shell.num_threads_),
  analyse_mode_(shell.analyse_mode_),
  output_listeners_(shell.output_listeners_) {
  }

  // ムーブコンストラクタ。
  UCIShell::UCIShell(UCIShell&& shell) :
  uci_command_(std::move(shell.uci_command_)),
  engine_ptr_(shell.engine_ptr_),
  table_ptr_(std::move(shell.table_ptr_)),
  moves_to_search_(std::move(shell.moves_to_search_)),
  table_size_(shell.table_size_),
  enable_pondering_(shell.enable_pondering_),
  num_threads_(shell.num_threads_),
  analyse_mode_(shell.analyse_mode_),
  output_listeners_(std::move(shell.output_listeners_)) {
  }

  // コピー代入。
  UCIShell& UCIShell::operator=(const UCIShell& shell) {
    uci_command_ = shell.uci_command_;
    engine_ptr_ = shell.engine_ptr_;
    *table_ptr_ = *(shell.table_ptr_);
    moves_to_search_ = shell.moves_to_search_;
    table_size_ = shell.table_size_;
    enable_pondering_ = shell.enable_pondering_;
    num_threads_ = shell.num_threads_;
    analyse_mode_ = shell.analyse_mode_;
    output_listeners_ = shell.output_listeners_;
    return *this;
  }

  // ムーブ代入。
  UCIShell& UCIShell::operator=(UCIShell&& shell) {
    uci_command_ = std::move(shell.uci_command_);
    engine_ptr_ = shell.engine_ptr_;
    table_ptr_ = std::move(shell.table_ptr_);
    moves_to_search_ = std::move(shell.moves_to_search_);
    table_size_ = shell.table_size_;
    enable_pondering_ = shell.enable_pondering_;
    num_threads_ = shell.num_threads_;
    analyse_mode_ = shell.analyse_mode_;
    output_listeners_ = std::move(shell.output_listeners_);
    return *this;
  }

  // デストラクタ。
  UCIShell::~UCIShell() {
    if (thinking_thread_.joinable()) {
      thinking_thread_.join();
    }
  }

  /********************/
  /* パブリック関数。 */
  /********************/
  // UCIコマンドを実行する。("quit"以外。)
  void UCIShell::InputCommand(const std::string input) {
    // コマンド実行。
    uci_command_(input);
  }

  // UCI出力を受け取る関数を登録する。
  void UCIShell::AddOutputListener
  (std::function<void(const std::string&)> func) {
    output_listeners_.push_back(func);
  }

  // PV情報を標準出力に送る。
  void UCIShell::PrintPVInfo(int depth, int seldepth, int score,
  Chrono::milliseconds time, std::uint64_t num_nodes, PVLine& pv_line) {
    std::ostringstream sout;
    sout << "info";
    sout << " depth " << depth;
    sout << " seldepth " << seldepth;
    sout << " score ";
    if (pv_line.ply_mate() >= 0) {
      int winner = pv_line.ply_mate() % 2;
      if (winner == 1) {
        // エンジンがメイトした。
        sout << "mate " << (pv_line.ply_mate() / 2) + 1;
      } else {
        // エンジンがメイトされた。
        sout << "mate -" << pv_line.ply_mate() / 2;
      }
    } else {
      sout << "cp " << score;
    }
    sout << " time " << time.count();
    sout << " nodes " << num_nodes;
    // PVラインを送る。
    sout << " pv";
    for (std::size_t i = 0; i < pv_line.length(); i++) {
      if (!(pv_line.line()[i])) break;

      sout << " " << TransMoveToString(pv_line.line()[i]);
    }

    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }
  }

  // 深さ情報を標準出力に送る。
  void UCIShell::PrintDepthInfo(int depth) {
    std::ostringstream sout;
    sout << "info depth " << depth;
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }
  }

  // 現在探索している手の情報を標準出力に送る。
  void UCIShell::PrintCurrentMoveInfo(Move move, int move_num) {
    std::ostringstream sout;
    // 手の情報を送る。
    sout << "info currmove " << TransMoveToString(move);

    // 手の番号を送る。
    sout << " currmovenumber " << move_num;

    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }
  }

  // その他の情報を標準出力に送る。
  void UCIShell::PrintOtherInfo(Chrono::milliseconds time,
  std::uint64_t num_nodes, int hashfull) {
    std::ostringstream sout;

    int time_2 = time.count();
    if (time_2 <= 0) time_2 = 1;
    sout << "info time " << time_2;
    sout << " nodes " << num_nodes;
    sout << " hashfull " << hashfull;
    sout << " nps " << (num_nodes * 1000) / time_2;

    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }
  }

  // 思考スレッド。
  void UCIShell::ThreadThinking() {
    // アナライズモードならトランスポジションテーブルを初期化。
    if (analyse_mode_) {
      table_ptr_.reset(new TranspositionTable(table_size_));
    }

    // テーブルの年齢の増加。
    table_ptr_->GrowOld();

    // 思考開始。
    PVLine pv_line = engine_ptr_->Calculate (num_threads_,
    *(table_ptr_.get()), moves_to_search_, *this);

    // 最善手を表示。
    std::ostringstream sout;

    sout << "bestmove ";
    if (pv_line.length() >= 1) {
      std::string move_str = TransMoveToString(pv_line.line()[0]);
      sout << move_str;

      // 2手目があるならponderで表示。
      if (pv_line.length() >= 2) {
        move_str = TransMoveToString(pv_line.line()[1]);
        sout << " ponder " << move_str;
      }
    }
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }
  }

  /*********************/
  /* UCIコマンド関数。 */
  /*********************/
  // uciコマンド。
  void UCIShell::CommandUCI(UCICommand::CommandArgs& args) {
    std::ostringstream sout;

    // IDを表示。
    sout << "id name " << ID_NAME;
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    sout.str("");
    sout << "id author " << ID_AUTHOR;
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    // 変更可能オプションの表示。
    // トランスポジションテーブルのサイズの変更。
    sout.str("");
    sout << "option name Hash type spin default "
    << (UCI_DEFAULT_TABLE_SIZE / (1024 * 1024)) << " min "
    << UCI_MIN_TABLE_SIZE / (1024 * 1024) << " max "
    << UCI_MAX_TABLE_SIZE / (1024 * 1024);
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    // トランスポジションテーブルの初期化。
    sout.str("");
    sout << "option name Clear Hash type button";
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    // ポンダリングできるかどうか。
    sout.str("");
    sout << "option name Ponder type check default ";
    if (UCI_DEFAULT_PONDER) {
      sout << "true";
    } else {
      sout << "false";
    }
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    // スレッドの数。
    sout.str("");
    sout << "option name Threads type spin default "
    << UCI_DEFAULT_THREADS << " min " << 1 << " max " << UCI_MAX_THREADS;
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    // アナライズモード。
    sout.str("");
    sout << "option name UCI_AnalyseMode type check default ";
    if (UCI_DEFAULT_ANALYSE_MODE) sout << "true";
    else sout << "false";
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func(sout.str());
    }

    // オーケー。
    // 出力関数に送る。
    for (auto& func : output_listeners_) {
      func("uciok");
    }

    // オプションの初期設定。
    table_size_ = UCI_DEFAULT_TABLE_SIZE;
    table_ptr_.reset(new TranspositionTable(table_size_));
    enable_pondering_ = UCI_DEFAULT_PONDER;
    num_threads_ = UCI_DEFAULT_THREADS;

  }

  // isreadyコマンド。
  void UCIShell::CommandIsReady(UCICommand::CommandArgs& args) {
    for (auto& func : output_listeners_) {
      func("readyok");
    }
  }

  // setoptionコマンド。
  void UCIShell::CommandSetOption(UCICommand::CommandArgs& args) {
    // nameとvalueがあるかどうか。
    // なければ設定できない。
    if ((args.find("name") == args.end())
    || (args.find("value") == args.end())) {
      return;
    }

    // nameの文字列をくっつける。
    std::string name_str = "";
    for (unsigned int i = 1; i < args["name"].size(); i++) {
      name_str += args["name"][i] + " ";
    }
    name_str.pop_back();

    // nameの文字を全部小文字にする。
    for (auto& c : name_str) c = std::tolower(c);

    // nameごとの処理。
    if (name_str == "hash") {
      // トランスポジションテーブルのサイズ変更。
      try {
        table_size_ = std::stoull(args["value"][1]) * 1024ULL * 1024ULL;

        table_size_ = table_size_ >= UCI_MIN_TABLE_SIZE
        ? table_size_ : UCI_MIN_TABLE_SIZE;

        table_size_ = table_size_ <= UCI_MAX_TABLE_SIZE
        ? table_size_ : UCI_MAX_TABLE_SIZE;

        table_ptr_.reset(new TranspositionTable(table_size_));
      } catch (...) {
        // 無視。
      }
    } else if (name_str == "clear hash") {
      // トランスポジションテーブルの初期化。
      table_ptr_.reset(new TranspositionTable(table_size_));
    } else if (name_str == "ponder") {
      // Ponderの有効化、無効化。
      if (args["value"][1] == "true") enable_pondering_ = true;
      else if (args["value"][1] == "false") enable_pondering_ = false;
    } else if (name_str == "threads") {
      // スレッドの数の変更。
      try {
        num_threads_ = std::stoi(args["value"][1]);

        num_threads_ = num_threads_ >= 1 ? num_threads_ : 1;
        num_threads_ = num_threads_ <= UCI_MAX_THREADS
        ?  num_threads_ : UCI_MAX_THREADS;
      } catch (...) {
        // 無視。
      }
    } else if (name_str == "uci_analysemode") {
      // アナライズモードの有効化、無効化。
      if (args["value"][1] == "true") analyse_mode_ = true;
      else if (args["value"][1] == "false") analyse_mode_ = false;
    }
  }

  // ucinewgameコマンド。
  void UCIShell::CommandUCINewGame(UCICommand::CommandArgs& args) {
    engine_ptr_->SetNewGame();
    table_ptr_.reset(new TranspositionTable(table_size_));
  }

  // positionコマンド。
  void UCIShell::CommandPosition(UCICommand::CommandArgs& args) {
    // startposコマンド。
    if (args.find("startpos") != args.end()) {
      engine_ptr_->SetStartPosition();
    }

    // fenコマンド。
    if (args.find("fen") != args.end()) {
      // fenコマンドをくっつける。
      std::string fen_str = "";
      for (unsigned int i = 1; i < args["fen"].size(); i++) {
        fen_str += args["fen"][i] + " ";
      }
      fen_str.pop_back();

      // エンジンに読み込む。
      engine_ptr_->LoadFen(Fen(fen_str));
    }

    // movesコマンド。
    if (args.find("moves") != args.end()) {
      // 手を指していく。
      for (unsigned int i = 1; i < args["moves"].size(); i++) {
        Move move = TransStringToMove(args["moves"][i]);
        if (move) engine_ptr_->PlayMove(move);
      }
    }
  }

  // goコマンド。
  void UCIShell::CommandGo(UCICommand::CommandArgs& args) {
    // 準備。
    std::uint32_t max_depth = MAX_PLYS;
    std::uint64_t max_nodes = MAX_NODES;
    Chrono::milliseconds thinking_time(-1U >> 1);
    bool infinite_thinking = false;
    moves_to_search_.clear();

    // 思考スレッドを終了させる。
    engine_ptr_->StopCalculation();
    if (thinking_thread_.joinable()) {
      thinking_thread_.join();
    }

    // searchmovesコマンド。
    if (args.find("searchmoves") != args.end()) {
      for (unsigned int i = 1; i < args["searchmoves"].size(); i++) {
        Move move = TransStringToMove(args["searchmoves"][i]);
        if (move) moves_to_search_.push_back(move);
      }
    }

    // ponderコマンド。
    if (args.find("ponder") != args.end()) {
      infinite_thinking = true;
    }

    // wtimeコマンド。
    if (args.find("wtime") != args.end()) {
      // 10分以上あるなら1分考える。5分未満なら持ち時間の10分の1。
      if (engine_ptr_->to_move() == WHITE) {
        try {
          Chrono::milliseconds time_control =
          Chrono::milliseconds(std::stoull(args["wtime"][1]));
          if (time_control.count() >= 600000) {
            thinking_time = Chrono::milliseconds(60000);
          } else {
            thinking_time = time_control / 10;
          }
        } catch (...) {
          // 無視。
        }
      }
    }

    // btimeコマンド。
    if (args.find("btime") != args.end()) {
      // 10分以上あるなら1分考える。5分未満なら持ち時間の10分の1。
      if (engine_ptr_->to_move() == BLACK) {
        try {
          Chrono::milliseconds time_control =
          Chrono::milliseconds(std::stoull(args["btime"][1]));
          if (time_control.count() >= 600000) {
            thinking_time = Chrono::milliseconds(60000);
          } else {
            thinking_time = time_control / 10;
          }
        } catch (...) {
          // 無視。
        }
      }
    }

    // wincコマンド。
    if (args.find("winc") != args.end()) {
      // 何もしない。
    }

    // bincコマンド。
    if (args.find("binc") != args.end()) {
      // 何もしない。
    }

    // movestogoコマンド。
    if (args.find("movestogo") != args.end()) {
      // 何もしない。
    }

    // depthコマンド。
    if (args.find("depth") != args.end()) {
      try {
        max_depth = std::stoi(args["depth"][1]);
        if (max_depth > MAX_PLYS) max_depth = MAX_PLYS;
      } catch (...) {
        // 無視。
      }
    }

    // nodesコマンド。
    if (args.find("nodes") != args.end()) {
      try {
        max_nodes = std::stoull(args["nodes"][1]);
        if (max_nodes > MAX_NODES) max_nodes = MAX_NODES;
      } catch (...) {
        // 無視。
      }
    }

    // mateコマンド。
    if (args.find("mate") != args.end()) {
      try {
        max_depth = (std::stoi(args["mate"][1]) * 2) - 1;
        if (max_depth > MAX_PLYS) max_depth = MAX_PLYS;
      } catch (...) {
        // 無視。
      }
    }

    // movetimeコマンド。
    if (args.find("movetime") != args.end()) {
      try {
        thinking_time = Chrono::milliseconds(std::stoull(args["movetime"][1]));
      } catch (...) {
        // 無視。
      }
    }

    // infiniteコマンド。
    if (args.find("infinite") != args.end()) {
      infinite_thinking = true;
    }

    // 別スレッドで思考開始。
    engine_ptr_->SetStopper(max_depth, max_nodes, thinking_time,
    infinite_thinking);
    thinking_thread_ =
    std::thread(&UCIShell::ThreadThinking, this);
  }

  // stopコマンド。
  void UCIShell::CommandStop(UCICommand::CommandArgs& args) {
    // 思考スレッドを終了させる。
    if (thinking_thread_.joinable()) {
      engine_ptr_->StopCalculation();
      thinking_thread_.join();
    }
  }

  // ponderhitコマンド。
  void UCIShell::CommandPonderHit(UCICommand::CommandArgs& args) {
    engine_ptr_->EnableInfiniteThinking(false);
  }

  /**************/
  /* 便利関数。 */
  /**************/
  // 手を文字列に変換。
  std::string UCIShell::TransMoveToString(Move move) {
    // 文字列ストリーム。
    std::ostringstream oss;

    // ストリームに流しこむ。
    Square from = move_from(move);
    Square to = move_to(move);
    oss << static_cast<char>(Util::GetFyle(from) + 'a');
    oss << static_cast<char>(Util::GetRank(from) + '1');
    oss << static_cast<char>(Util::GetFyle(to) + 'a');
    oss << static_cast<char>(Util::GetRank(to) + '1');
    switch (move_promotion(move)) {
      case KNIGHT:
        oss << 'n';
        break;
      case BISHOP:
        oss << 'b';
        break;
      case ROOK:
        oss << 'r';
        break;
      case QUEEN:
        oss << 'q';
        break;
      default:
        break;
    }

    return oss.str();
  }

  // 文字列を手に変換。
  Move UCIShell::TransStringToMove(std::string move_str) {
    Move move = 0;

    if (move_str.size() < 4) return 0;

    // fromをパース。
    Square from = 0;
    if ((move_str[0] < 'a') || (move_str[0] > 'h')) {
      return 0;
    }
    from |= move_str[0] - 'a';
    if ((move_str[1] < '1') || (move_str[1] > '8')) {
      return 0;
    }
    from |= (move_str[1] - '1') << 3;
    move_from(move, from);

    // toをパース。
    Square to = 0;
    if ((move_str[2] < 'a') || (move_str[2] > 'h')) {
      return 0;
    }
    to |= move_str[2] - 'a';
    if ((move_str[3] < '1') || (move_str[3] > '8')) {
      return 0;
    }
    to |= (move_str[3] - '1') << 3;
    move_to(move, to);

    // 昇格をパース。
    if (move_str.size() >= 5) {
      switch (move_str[4]) {
        case 'n':
          move_promotion(move, KNIGHT);
          break;
        case 'b':
          move_promotion(move, BISHOP);
          break;
        case 'r':
          move_promotion(move, ROOK);
          break;
        case 'q':
          move_promotion(move, QUEEN);
          break;
        default:
          break;
      }
    }

    return move;
  }

  /********************/
  /* UCICommand関連。 */
  /********************/
  // コマンドごとのコールバック関数を登録する。
  void UCICommand::Add(const std::string& command_name,
  const std::vector<std::string>& subcommand_name_vec,
  std::function<void(CommandArgs&)> func) {
    for (auto& command_func : func_vec_) {
      if (command_func.command_name_ == command_name) {
        command_func.subcommand_name_vec_ = subcommand_name_vec;
        command_func.func_ = func;
        return;
      }
    }

    func_vec_.push_back(CommandFunction
    {command_name, subcommand_name_vec, func});
  }

  // コマンドを実行する。
  void UCICommand::operator()(const std::string& command_line) {
    // トークンに分ける。
    std::vector<std::string> tokens = Util::Split(command_line, " ", "");

    // コマンドを探す。
    CommandArgs args;
    for (auto& command_func : func_vec_) {
      if (command_func.command_name_ == tokens[0]) {
        // コマンドラインをパース。
        std::string temp = "";
        for (auto& word : tokens) {
          for (auto& subcommand_name :
          command_func.subcommand_name_vec_) {
            if (word == subcommand_name) {
              temp = word;
              break;
            }
          }
          args[temp].push_back(word);
        }

        // コマンドを実行。
        command_func.func_(args);
        return;
      }
    }
  }
}  // namespace Sayuri
