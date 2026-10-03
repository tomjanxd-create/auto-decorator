#pragma once

#include <Geode/Geode.hpp>
#include <vector>

// A decoration style. Decoration pieces are referenced by their sprite frame
// name and resolved to object IDs at runtime (see Decorator.cpp), so a piece
// that doesn't exist in the current GD version is simply skipped.
struct DecoStyle {
    char const* name;

    // Level settings
    int background;
    int ground;
    int groundLine;
    int middleground;

    // Start colors
    cocos2d::ccColor3B bg;
    cocos2d::ccColor3B groundColor;
    cocos2d::ccColor3B ground2;
    cocos2d::ccColor3B line;
    cocos2d::ccColor3B obj;

    // Decoration colors
    cocos2d::ccColor3B topColor;
    cocos2d::ccColor3B bottomColor;
    cocos2d::ccColor3B backColor;
    cocos2d::ccColor3B groundDecoColor;
    float backOpacity;
    bool backBlending;

    // Pieces
    std::vector<char const*> top;      // sits on top of blocks
    std::vector<char const*> bottom;   // hangs under ceilings
    std::vector<char const*> floor;    // sits on the ground
    std::vector<char const*> back;     // large background art

    // Density (0..1 chance per surface tile)
    float topChance;
    float bottomChance;
    float floorChance;
    float backSpacing;   // average distance between background pieces, in blocks
    float backScaleMin;
    float backScaleMax;
};

std::vector<DecoStyle> const& getDecoStyles();
