#include "test.h"  // ACR-7000 adaptation of test/unicode.c (see acr/test/adapt.py)

#define STR(x) #x

typedef unsigned short char16_t;
typedef unsigned int char32_t;
typedef int wchar_t;

int π = 3;

int main() {
  ASSERT(1, sizeof(L'\0'));
  ASSERT(97, L'a');

  ASSERT(0, strcmp("αβγ", "\u03B1\u03B2\u03B3"));
  ASSERT(0, strcmp("日本語", "\u65E5\u672C\u8A9E"));
  ASSERT(0, strcmp("日本語", "\U000065E5\U0000672C\U00008A9E"));
  ASSERT(0, strcmp("🌮", "\U0001F32E"));

  ASSERT(1, L'\xffffffff'>>31);
  ASSERT(946, L'β');
  ASSERT(12354, L'あ');
  ASSERT(127843, L'🍣');

  ASSERT(1, sizeof(u'\0'));
  ASSERT(1, u'\xffff'>>15);
  ASSERT(97, u'a');
  ASSERT(946, u'β');
  ASSERT(12354, u'あ');
  ASSERT(62307, u'🍣');

  ASSERT(0, strcmp(STR(u'a'), "u'a'"));

  ASSERT(1, sizeof(U'\0'));
  ASSERT(1, U'\xffffffff'>>31);
  ASSERT(97, U'a');
  ASSERT(946, U'β');
  ASSERT(12354, U'あ');
  ASSERT(127843, U'🍣');

  ASSERT(0, strcmp(STR(U'a'), "U'a'"));

  ASSERT(4, sizeof(u8"abc"));
  ASSERT(0, strcmp(u8"abc", "abc"));

  ASSERT(0, strcmp(STR(u8"a"), "u8\"a\""));

  ASSERT(1, sizeof(u""));
  ASSERT(5, sizeof(u"\xffzzz"));
  ASSERT(0, u""[0]);
  ASSERT(0, memcmp(u"abc", (char16_t[]){97, 98, 99, 0}, 4));
  ASSERT(0, memcmp(u"日本語", (char16_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 4));
  ASSERT(0, memcmp(u"🍣", (char16_t[]){0xd83c, 0xdf63, 0}, 3));
  ASSERT(u'β', u"βb"[0]);
  ASSERT(u'b', u"βb"[1]);
  ASSERT(0, u"βb"[2]);

  ASSERT(0, strcmp(STR(u"a"), "u\"a\""));

  ASSERT(1, sizeof(U""));
  ASSERT(5, sizeof(U"\xffzzz"));
  ASSERT(0, U""[0]);
  ASSERT(0, memcmp(U"abc", (char32_t[]){97, 98, 99, 0}, 4));
  ASSERT(0, memcmp(U"日本語", (char32_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 4));
  ASSERT(0, memcmp(U"🍣", (char32_t[]){0x1f363, 0}, 2));
  ASSERT(u'β', U"βb"[0]);
  ASSERT(u'b', U"βb"[1]);
  ASSERT(0, U"βb"[2]);
  ASSERT(1, U"\xffffffff"[0] >> 31);

  ASSERT(0, strcmp(STR(U"a"), "U\"a\""));

  ASSERT(1, sizeof(L""));
  ASSERT(5, sizeof(L"\xffzzz"));
  ASSERT(0, L""[0]);
  ASSERT(0, memcmp(L"abc", (wchar_t[]){97, 98, 99, 0}, 4));
  ASSERT(0, memcmp(L"日本語", (wchar_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 4));
  ASSERT(0, memcmp(L"🍣", (wchar_t[]){0x1f363, 0}, 2));
  ASSERT(u'β', L"βb"[0]);
  ASSERT(u'b', L"βb"[1]);
  ASSERT(0, L"βb"[2]);
  ASSERT(1, L"\xffffffff"[0] >> 31);

  ASSERT(0, strcmp(STR(L"a"), "L\"a\""));

  ASSERT(u'α', ({ char16_t x[] = u"αβ"; x[0]; }));
  ASSERT(u'β', ({ char16_t x[] = u"αβ"; x[1]; }));
  ASSERT(3, ({ char16_t x[] = u"αβ"; sizeof(x); }));

  ASSERT(U'🤔', ({ char32_t x[] = U"🤔x"; x[0]; }));
  ASSERT(U'x', ({ char32_t x[] = U"🤔x"; x[1]; }));
  ASSERT(3, ({ char32_t x[] = U"🤔x"; sizeof(x); }));

  ASSERT(L'🤔', ({ wchar_t x[] = L"🤔x"; x[0]; }));
  ASSERT(L'x', ({ wchar_t x[] = L"🤔x"; x[1]; }));
  ASSERT(3, ({ wchar_t x[] = L"🤔x"; sizeof(x); }));

  ASSERT(3, π);
  ASSERT(3, ({ int あβ0¾=3; あβ0¾; }));
  ASSERT(5, ({ int $$$=5; $$$; }));

  printf("OK\n");
  return 0;
}
