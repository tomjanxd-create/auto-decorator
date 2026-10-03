#pragma once

#include "Styles.hpp"

struct DecorateOptions {
    bool clearOld = true;
    bool applyColors = true;
};

struct DecorateResult {
    int placed = 0;
    int removed = 0;
};

// Every object placed by the decorator is put in this group, so it can be
// found and removed again later.
constexpr int DECO_GROUP = 9999;

DecorateResult decorateLevel(LevelEditorLayer* editor, DecoStyle const& style, DecorateOptions const& options);
int removeDecoration(LevelEditorLayer* editor);
