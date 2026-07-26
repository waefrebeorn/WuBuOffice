/* spell_seed.c -- built-in common English word list (generated).
 * A small, high-frequency seed so the checker is useful with zero config.
 * Extend via spell_load() with a full word list for production. */
#include "spell.h"

static const char *const SEED[] = {
    "brown", "fox", "jumps", "over", "lazy", "dog", "quick", "cat", "runs", "fast",
    "the", "be", "to", "of", "and", "a", "in", "that", "have", "i",
    "it", "for", "not", "on", "with", "he", "as", "you", "do", "at",
    "this", "but", "his", "by", "from", "they", "we", "say", "her", "she",
    "or", "an", "will", "my", "one", "all", "would", "there", "their", "what",
    "so", "up", "out", "if", "about", "who", "get", "which", "go", "me",
    "when", "make", "can", "like", "time", "no", "just", "him", "know", "take",
    "people", "into", "year", "your", "good", "some", "could", "them", "see", "other",
    "than", "then", "now", "look", "only", "come", "its", "over", "think", "also",
    "back", "after", "use", "two", "how", "our", "work", "first", "well", "way",
    "even", "new", "want", "because", "any", "these", "give", "day", "most", "us",
    "is", "are", "was", "were", "been", "has", "had", "did", "said", "find",
    "tell", "ask", "seem", "feel", "try", "leave", "call", "last", "long", "great",
    "little", "own", "old", "right", "big", "high", "different", "small", "large", "next",
    "early", "young", "important", "few", "public", "bad", "same", "able", "man", "government",
    "company", "number", "group", "problem", "fact", "hello", "world", "document", "text", "editor",
    "spell", "check", "word", "words", "letter", "page", "format", "font", "size", "bold",
    "italic", "underline", "paragraph", "sentence", "line", "insert", "delete", "select", "copy", "paste",
    "cut", "save", "open", "close", "file", "print", "export", "import", "table", "row",
    "column", "cell", "header", "footer", "margin", "align", "center", "left", "justify", "color",
    "style", "heading", "title", "subtitle", "list", "bullet", "outline", "review", "comment", "track",
    "change", "accept", "reject", "replace", "search", "language", "dictionary", "suggestion", "correct", "wrong",
    "error", "mistake", "grammar", "capital", "lower", "upper", "case", "space", "tab", "enter",
    "return", "break", "section", "chapter", "appendix", "index", "reference", "footnote", "endnote", "citation",
    "bibliography",
};

int spell_seed_english(SpellDict *d) {
    int n = (int)(sizeof(SEED) / sizeof(SEED[0]));
    int added = 0;
    for (int i = 0; i < n; i++) added += spell_add_word(d, SEED[i]);
    return added;
}
