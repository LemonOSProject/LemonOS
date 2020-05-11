#pragma once

#include <gfx/graphics.h>
#include <list.h>

#define GET_LEFT(rect) (rect.pos.x)
#define GET_RIGHT(rect) (rect.pos.x + rect.size.x)
#define GET_TOP(rect) (rect.pos.y)
#define GET_BOTTOM(rect) (rect.pos.y + rect.size.y)

void SplitRect(rect_t victim, rect_t cutter, List<rect_t>* rects){

    if(GET_LEFT(cutter) > GET_LEFT(victim) && GET_LEFT(cutter) <= GET_RIGHT(victim)){
        rects->add_back({victim.pos, {(GET_LEFT(cutter) - GET_LEFT(victim)) - 1, victim.size.y}}); // Split rect vertically on the left
        
        victim.size.x -= (GET_LEFT(cutter) - victim.pos.x);
        victim.pos.x = GET_LEFT(cutter);
    }
    
    if(GET_TOP(cutter) > GET_TOP(victim) && GET_TOP(cutter) <= GET_BOTTOM(victim)){
        rects->add_back({victim.pos, {victim.size.x, (cutter.pos.y - victim.pos.y) - 1}}); // Split rect vertically on the left
        
        victim.size.y -= (GET_TOP(cutter) - victim.pos.y);
        victim.pos.y = GET_TOP(cutter);
    }

    if(GET_RIGHT(cutter) >= GET_LEFT(victim) && GET_RIGHT(cutter) < GET_RIGHT(victim)){
        rects->add_back({{GET_RIGHT(cutter) + 1, victim.pos.y},{GET_RIGHT(victim) - (GET_RIGHT(cutter) + 1), victim.size.y}});

        victim.size.x = (GET_RIGHT(cutter) - victim.pos.x);
    }
    
    if(GET_BOTTOM(cutter) >= GET_TOP(victim) && GET_BOTTOM(cutter) < GET_BOTTOM(victim)){
        rects->add_back({{victim.pos.x, GET_BOTTOM(cutter) + 1}, {victim.size.x, GET_BOTTOM(victim) - (GET_BOTTOM(cutter) + 1)}}); // Split rect vertically on the left

        victim.size.y = (GET_BOTTOM(cutter) - victim.pos.y);
    }
}