#include <string.h>
#include "keyboard/fw2kb.h"
#include "test_util.h"

/* Pages reachable by cycling in FW2KB_MODE_ALL: LOWER, NUMBERS, UPPER.
 * Duplicated here on purpose — the test asserts against the documented user
 * model, not against fw2kb.c's private table. */
static const char *k_reachable[] = {
    "abcde", "fghij", "klmno", "pqrst", "uvwxy",       /* PAGE_LOWER   */
    "01234", "56789", "+^.<>", "=*/~-", "()[]z",       /* PAGE_NUMBERS */
    "ABCDE", "FGHIJ", "KLMNO", "PQRST", "UVWXY",       /* PAGE_UPPER   */
};

/* Resolve ch, replay the presses, and assert fw2kb emits exactly that char. */
static void expect_types(fw2kb_t *kb, char ch)
{
    fw2kb_btn seq[FW2KB_CHORD_MAX];
    int n = 0;
    if (!fw2kb_chord_for(kb, ch, seq, &n)) {
        printf("FAIL: no chord for '%c' (0x%02X)\n", ch, (unsigned char)ch);
        g_failures++;
        return;
    }
    ASSERT_TRUE(n >= 2 && n <= FW2KB_CHORD_MAX);
    for (int i = 0; i < n; i++) fw2kb_press(kb, seq[i]);

    fw2kb_event ev;
    if (!fw2kb_next_event(kb, &ev)) {
        printf("FAIL: no event for '%c'\n", ch);
        g_failures++;
        return;
    }
    ASSERT_EQ(ev.key, FW2KB_KEY_CHAR);
    ASSERT_EQ(ev.ch, ch);
    /* the chord must leave the keyboard ready for the next character */
    ASSERT_TRUE(!fw2kb_in_chord(kb));
}

int main(void)
{
    fw2kb_t kb;

    /* Property: every character on every reachable page resolves and replays,
     * from whatever state the previous character left behind. */
    fw2kb_init(&kb);
    for (size_t g = 0; g < sizeof k_reachable / sizeof k_reachable[0]; g++)
        for (const char *c = k_reachable[g]; *c; c++)
            expect_types(&kb, *c);

    /* Resolving mid-chord emits a PAGE press first to cancel, and still lands
     * on the right character. */
    fw2kb_init(&kb);
    fw2kb_press(&kb, FW2KB_BTN_GRAY);        /* half-entered chord */
    ASSERT_TRUE(fw2kb_in_chord(&kb));
    expect_types(&kb, 'a');

    /* Typing a whole word works end to end. */
    fw2kb_init(&kb);
    const char *word = "hello";
    for (const char *c = word; *c; c++) expect_types(&kb, *c);

    /* Unresolvable inputs are rejected, not silently mis-typed. */
    fw2kb_btn seq[FW2KB_CHORD_MAX];
    int n = 0;
    fw2kb_init(&kb);
    ASSERT_TRUE(!fw2kb_chord_for(&kb, '\0', seq, &n));
    ASSERT_TRUE(!fw2kb_chord_for(&kb, 'z' + 1, seq, &n));  /* '{' is on SYMBOLS */

    /* chord_for must not mutate the keyboard it inspects. */
    fw2kb_init(&kb);
    fw2kb_t before = kb;
    ASSERT_TRUE(fw2kb_chord_for(&kb, 'q', seq, &n));
    ASSERT_EQ(memcmp(&before, &kb, sizeof kb), 0);

    TEST_RETURN();
}
