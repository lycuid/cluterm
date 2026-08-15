#ifndef __CLUTERM__ACTIONS__OSC_H__
#define __CLUTERM__ACTIONS__OSC_H__

#include <cluterm.h>
#include <stdint.h>

EXPORT void osc_execute(Cluterm *term, OSC_Payload *osc)
{
    if (term->osc_handler)
        term->osc_handler(term, osc);
}

#endif
