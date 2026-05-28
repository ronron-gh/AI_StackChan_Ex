#include "Phrases.h"
#include <Arduino.h>

namespace phrases {

namespace {
template <int N>
const char* pick(const char* const (&arr)[N]) {
  return arr[random(0, N)];
}
}  // namespace

const char* listening() {
  static const char* const arr[] = {
    "なになに？",
    "聞いてるよー",
    "御用なのだー？",
    "どうしたー？",
    "はーい！",
  };
  return pick(arr);
}

const char* followup() {
  static const char* const arr[] = {
    "他にもあるー？",
    "まだあるー？",
    "なーに？",
    "ぼくに聞いてー！",
    "うん、続けてー",
  };
  return pick(arr);
}

const char* thinking() {
  static const char* const arr[] = {
    "うーん…",
    "ちょっと待ってねー",
    "考え中なのだー",
    "んーー…",
    "えーっと…",
  };
  return pick(arr);
}

const char* not_heard() {
  static const char* const arr[] = {
    "もう一回言ってー？",
    "聞こえなかったー…",
    "んっ、なんて？",
    "ごめーん、もう一度ー",
  };
  return pick(arr);
}

const char* head_pet() {
  static const char* const arr[] = {
    "えへへー",
    "うふふー",
    "気持ちいいのだー",
    "もっと撫でてー",
    "やー、うれしいー",
  };
  return pick(arr);
}

const char* error() {
  static const char* const arr[] = {
    "うー、エラーなのだー",
    "ごめんなさい、エラーー",
    "何かおかしいよー",
    "ぼく調子悪いみたいー…",
  };
  return pick(arr);
}

const char* unknown() {
  static const char* const arr[] = {
    "わかんないー",
    "うーん、わからないのだー",
    "ぼくにはむずかしいー…",
    "それは知らないよー",
  };
  return pick(arr);
}

const char* greeting() {
  static const char* const arr[] = {
    "やー、スタックチャンなのだー！",
    "こんにちはー！",
    "ぼく、スタックチャンー",
    "いっぱい話そうねー！",
  };
  return pick(arr);
}

}  // namespace phrases
