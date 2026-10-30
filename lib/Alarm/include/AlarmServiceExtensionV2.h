#ifndef UTILITIES_ALARM_SERVICE_EXTENSION_V2_H
#define UTILITIES_ALARM_SERVICE_EXTENSION_V2_H
#include "AlarmClockEditV2.h"
#include <string.h>
/* One selected, tagged extension follows the negotiated common prefix. The
 * tag/version belongs to its owner; unknown families never alias callbacks.
 * Existing descriptor and editor layouts remain unchanged. */
static inline const void *alarm_service_extension_v2(const alarm_service_v1 *api,
    size_t bytes,uint32_t tag,uint32_t version) {
    const alarm_service_descriptor_v2 *d=alarm_service_descriptor(api);
    if(!d||bytes<2*sizeof(uint32_t))return NULL;
    size_t offset=sizeof(*d);
    if(d->features&ALARM_DESCRIPTOR_CLOCK_EDIT) {
        if(!alarm_clock_edit_descriptor(api))return NULL;
        offset=sizeof(alarm_clock_edit_descriptor_v2);
    }
    if(api->struct_size<offset||bytes>api->struct_size-offset)return NULL;
    const unsigned char *tail=(const unsigned char *)api+offset;
    uint32_t header[2];memcpy(header,tail,sizeof(header));
    return header[0]==tag&&header[1]==version?tail:NULL;
}
#endif
